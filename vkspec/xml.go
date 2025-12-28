/*
Copyright 2025 The goARRG Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package vkspec

import (
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"slices"
	"strings"

	"goarrg.com/toolchain"
	"goarrg.com/toolchain/cgodep"
)

type xmlParserInterface struct {
	findNextElement func() (xml.StartElement, error)
	findNextString  func() (string, error)
	findElementEnd  func() (xml.EndElement, error)
	findAttribute   func(name string, attrs []xml.Attr) xml.Attr
	skip            func() error
}

type xmlTypes struct {
	handles map[string]Handle
}
type (
	xmlCommands   map[string]Command
	xmlExtensions map[string]Extension
)

func parseXML() map[string]any {
	parsers := map[string]func(xmlParserInterface) any{
		"types":      xmlParseTypes,
		"commands":   xmlParseCommands,
		"extensions": xmlParseExtensions,
	}

	docs := filepath.Join(cgodep.InstallDir("vulkan-docs", toolchain.Target{}, toolchain.BuildRelease), "xml", "vk.xml")
	fIn, err := os.Open(docs)
	if err != nil {
		panic(err)
	}

	result := map[string]any{}

	{
		decoder := xml.NewDecoder(fIn)
		var unProcessedToken xml.Token
		findNextElement := func() (xml.StartElement, error) {
			var err error
			t := unProcessedToken
			unProcessedToken = nil
			if t == nil {
				t, err = decoder.Token()
			}
			for {
				if err != nil {
					return xml.StartElement{}, err
				}
				if start, ok := t.(xml.StartElement); ok {
					return start, nil
				}
				if _, ok := t.(xml.EndElement); ok {
					return xml.StartElement{}, io.EOF
				}
				t, err = decoder.Token()
			}
		}
		findNextString := func() (string, error) {
			t, err := decoder.Token()
			if err != nil {
				return "", err
			}
			if c, ok := t.(xml.CharData); ok {
				return string(c), nil
			}
			unProcessedToken = t
			return "", io.EOF
		}
		findElementEnd := func() (xml.EndElement, error) {
			var err error
			t := unProcessedToken
			unProcessedToken = nil
			if t == nil {
				t, err = decoder.Token()
			}
			started := false
			for {
				if err != nil {
					return xml.EndElement{}, err
				}
				if _, ok := t.(xml.StartElement); ok {
					started = true
				}
				if end, ok := t.(xml.EndElement); ok {
					if started {
						started = false
						continue
					}
					return end, nil
				}
				t, err = decoder.Token()
			}
		}
		findAttribute := func(name string, attrs []xml.Attr) xml.Attr {
			for _, a := range attrs {
				if a.Name.Local == name {
					return a
				}
			}
			return xml.Attr{}
		}

		registry, err := findNextElement()
		if err != nil {
			panic(err)
		}
		if registry.Name.Local != "registry" {
			panic(fmt.Sprintf("unknown xml format %s", registry.Name.Local))
		}
		for {
			next, err := findNextElement()
			if err != nil {
				if errors.Is(err, io.EOF) {
					break
				}
				panic(err)
			}
			if fn, ok := parsers[next.Name.Local]; ok {
				result[next.Name.Local] = fn(xmlParserInterface{
					findNextElement: findNextElement,
					findNextString:  findNextString,
					findElementEnd:  findElementEnd,
					findAttribute:   findAttribute,
					skip:            decoder.Skip,
				})
			} else {
				decoder.Skip()
			}
		}
	}

	return result
}

func xmlParseTypes(vkxml xmlParserInterface) any {
	result := xmlTypes{
		handles: map[string]Handle{},
	}
	type xmlTypeParser struct {
		fn   func(xmlParserInterface, xml.StartElement, any)
		data any
	}
	parsers := map[string]xmlTypeParser{
		"handle": {
			fn:   xmlParseHandles,
			data: result.handles,
		},
	}
	for {
		start, err := vkxml.findNextElement()
		if err != nil {
			if errors.Is(err, io.EOF) {
				break
			}
			panic(err)
		}
		{
			category := vkxml.findAttribute("category", start.Attr)
			p, ok := parsers[category.Value]
			if ok {
				p.fn(vkxml, start, p.data)
				_, err = vkxml.findElementEnd()
				if err != nil {
					panic(err)
				}
			} else {
				err := vkxml.skip()
				if err != nil {
					panic(err)
				}
			}
		}
	}

	for k, t := range result.handles {
		if t.Alias != "" {
			alias := result.handles[t.Alias]
			for alias.Alias != "" {
				alias = result.handles[alias.Alias]
			}
			t.TypeName = alias.TypeName
			t.Parent = alias.Parent
			result.handles[k] = t
		}
	}

	return result
}

func xmlParseHandles(vkxml xmlParserInterface, start xml.StartElement, result any) {
	handles := result.(map[string]Handle)
	if vkxml.findAttribute("objtypeenum", start.Attr).Value == "" {
		name := vkxml.findAttribute("name", start.Attr).Value
		parent := vkxml.findAttribute("alias", start.Attr).Value
		if name != "" {
			handles[name] = Handle{
				Name:  name,
				Alias: parent,
			}
		}
		return
	}

	var kind, name string
	parent := vkxml.findAttribute("parent", start.Attr).Value

	{
		typeName, err := vkxml.findNextElement()
		if err != nil {
			panic(err)
		}
		if typeName.Name.Local != "type" {
			panic("unexpected xml structure")
		}
		kind, err = vkxml.findNextString()
		if err != nil {
			panic(err)
		}
		_, err = vkxml.findElementEnd()
		if err != nil {
			panic(err)
		}

		varName, err := vkxml.findNextElement()
		if err != nil {
			panic(err)
		}
		if varName.Name.Local != "name" {
			panic("unexpected xml structure")
		}
		name, err = vkxml.findNextString()
		if err != nil {
			panic(err)
		}
		_, err = vkxml.findElementEnd()
		if err != nil {
			panic(err)
		}
	}

	if name == "" {
		panic("Unexpected empty name")
	}

	handles[name] = Handle{
		Name:     name,
		TypeName: kind,
		Parent:   parent,
	}
}

func xmlParseCommands(vkxml xmlParserInterface) any {
	result := xmlCommands{}
	for {
		start, err := vkxml.findNextElement()
		if err != nil {
			if errors.Is(err, io.EOF) {
				break
			}
			panic(err)
		}
		{
			export := vkxml.findAttribute("export", start.Attr)
			if export.Value != "" && !slices.Contains(strings.Split(export.Value, ","), "vulkan") {
				if err := vkxml.skip(); err != nil {
					panic(err)
				}
				continue
			}

			alias := vkxml.findAttribute("alias", start.Attr)
			if alias.Value != "" {
				name := vkxml.findAttribute("name", start.Attr)
				result[name.Value] = Command{
					Name:  name.Value,
					Alias: alias.Value,
				}
				_, err = vkxml.findElementEnd()
				if err != nil {
					panic(err)
				}
				continue
			}

			cmd := Command{}

			proto, err := vkxml.findNextElement()
			if err != nil {
				panic(err)
			}
			if proto.Name.Local != "proto" {
				panic("unexpected xml structure")
			}
			{
				ret, err := vkxml.findNextElement()
				if err != nil {
					panic(err)
				}
				retStr, err := vkxml.findNextString()
				if err != nil {
					panic(err)
				}
				_, err = vkxml.findElementEnd()
				if err != nil {
					panic(err)
				}
				if ret.Name.Local != "type" {
					panic("unexpected xml structure")
				}

				name, err := vkxml.findNextElement()
				if err != nil {
					panic(err)
				}
				if name.Name.Local != "name" {
					panic("unexpected xml structure")
				}
				nameStr, err := vkxml.findNextString()
				if err != nil {
					panic(err)
				}
				_, err = vkxml.findElementEnd()
				if err != nil {
					panic(err)
				}

				cmd.ReturnType = retStr
				cmd.Name = nameStr
			}
			_, err = vkxml.findElementEnd()
			if err != nil {
				panic(err)
			}

			for {
				p := CommandParam{}
				param, err := vkxml.findNextElement()
				if err != nil {
					if errors.Is(err, io.EOF) {
						break
					}
					panic(err)
				}
				if param.Name.Local != "param" {
					if err := vkxml.skip(); err != nil {
						panic(err)
					}
					continue
				}
				constStr, err := vkxml.findNextString()
				if err != nil && err != io.EOF {
					panic(err)
				}
				if strings.TrimSpace(constStr) == "const" {
					p.IsReadOnly = true
				}
				{
					{
						typeName, err := vkxml.findNextElement()
						if err != nil {
							panic(err)
						}
						if typeName.Name.Local != "type" {
							panic("unexpected xml structure")
						}
						typeStr, err := vkxml.findNextString()
						if err != nil {
							panic(err)
						}
						_, err = vkxml.findElementEnd()
						if err != nil {
							panic(err)
						}
						p.TypeName = typeStr
					}

					{
						ptrStr, err := vkxml.findNextString()
						if err != nil && err != io.EOF {
							panic(err)
						}
						if strings.TrimSpace(ptrStr) == "*" {
							p.IsPointer = true
						}
					}

					{
						varName, err := vkxml.findNextElement()
						if err != nil {
							panic(err)
						}
						if varName.Name.Local != "name" {
							panic("unexpected xml structure")
						}
						varStr, err := vkxml.findNextString()
						if err != nil {
							panic(err)
						}
						_, err = vkxml.findElementEnd()
						if err != nil {
							panic(err)
						}
						p.VarName = varStr
					}

					cmd.Params = append(cmd.Params, p)
				}
				_, err = vkxml.findElementEnd()
				if err != nil {
					panic(err)
				}
			}
			result[cmd.Name] = cmd
		}
	}

	// fill aliased info, we have to do this last as the cmd may not have been processed first
	for _, cmd := range result {
		if cmd.Alias != "" {
			alias := result[cmd.Alias]
			for alias.Alias != "" {
				alias = result[alias.Alias]
			}
			cmd.ReturnType = alias.ReturnType
			cmd.Params = alias.Params
			result[cmd.Name] = cmd
		}
	}

	return result
}

func xmlParseExtensions(vkxml xmlParserInterface) any {
	extensions := xmlExtensions{}
	for {
		start, err := vkxml.findNextElement()
		if err != nil {
			if errors.Is(err, io.EOF) {
				break
			}
			panic(err)
		}
		{
			rootName := vkxml.findAttribute("name", start.Attr)
			rootKind := vkxml.findAttribute("type", start.Attr)
			rootDepends := vkxml.findAttribute("depends", start.Attr)
			rootSupported := vkxml.findAttribute("supported", start.Attr)
			rootPromoted := vkxml.findAttribute("promotedto", start.Attr)
			rootDeprecated := vkxml.findAttribute("deprecatedby", start.Attr)
			rootProvisional := vkxml.findAttribute("provisional", start.Attr)
			rootPlatform := vkxml.findAttribute("platform", start.Attr)

			e := Extension{
				Name:     rootName.Value,
				Kind:     rootKind.Value,
				Platform: rootPlatform.Value,
				Valid:    slices.Contains(strings.Split(rootSupported.Value, ","), "vulkan"),
				Promoted: rootPromoted.Value,
				Extends:  map[string][]string{},
			}
			{
				depends := strings.NewReplacer("(", "", ")", "", ",", "+").Replace(rootDepends.Value)
				if depends != "" {
					e.Depends = strings.Split(depends, "+")
				}
			}
			if rootProvisional.Value == "true" {
				e.Provisional = true
			}
			if rootDeprecated.Value != "" {
				if e.Promoted != "" {
					panic(fmt.Sprintf("unexpected promotion and deprecation for: %s", rootName.Value))
				}
				e.Deprecated = rootDeprecated.Value
			}
			for {
				node, err := vkxml.findNextElement()
				if err != nil {
					if errors.Is(err, io.EOF) {
						break
					}
					panic(err)
				}
				switch node.Name.Local {
				case "require":
					for {
						t, err := vkxml.findNextElement()
						if err != nil {
							if errors.Is(err, io.EOF) {
								break
							}
							panic(err)
						}
						_, err = vkxml.findElementEnd()
						if err != nil {
							panic(err)
						}

						switch t.Name.Local {
						case "enum":
							typename := vkxml.findAttribute("extends", t.Attr)
							if typename.Value == "VkObjectType" {
								valuename := vkxml.findAttribute("name", t.Attr)
								e.Handles = append(e.Handles, valuename.Value)
							} else if typename.Value != "" {
								valuename := vkxml.findAttribute("name", t.Attr)
								e.Extends[typename.Value] = append(e.Extends[typename.Value], valuename.Value)
							}
						case "type":
							typename := vkxml.findAttribute("name", t.Attr)
							if typename.Name.Local == "name" {
								e.Types = append(e.Types, typename.Value)
							}
						case "command":
							typename := vkxml.findAttribute("name", t.Attr)
							if typename.Name.Local == "name" {
								e.Commands = append(e.Commands, typename.Value)
							}
						}
					}

				default:
					err = vkxml.skip()
					if err != nil {
						panic(err)
					}
				}
			}
			extensions[e.Name] = e
		}
	}

	for k, e := range extensions {
		for i, h := range e.Handles {
			target := "VK" + strings.ReplaceAll(strings.TrimPrefix(h, "VK_OBJECT_TYPE_"), "_", "")

			for _, t := range e.Types {
				if target == strings.ToUpper(t) {
					e.Handles[i] = t
					break
				}
			}
		}
		extensions[k] = e
	}

	return extensions
}
