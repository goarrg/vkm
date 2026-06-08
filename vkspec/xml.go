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
	"unicode"

	"goarrg.com/debug"
	"goarrg.com/toolchain"
	"goarrg.com/toolchain/cgodep"
)

type xmlParserInterface struct {
	findNextElement func() (xml.StartElement, error)
	findNextString  func() (string, error)
	findElementEnd  func() (xml.EndElement, error)
	findAttribute   func(attrs []xml.Attr, names ...string) xml.Attr
	skip            func() error
	line            func() int
}
type xmlExtension struct {
	Extension
	DependentExports map[string]Exports
}
type xmlTypes struct {
	handles map[string]Handle
	structs map[string]Struct
	unions  map[string]Union
}
type (
	xmlPlatforms  map[string]Platform
	xmlEnums      map[string][]string
	xmlCommands   map[string]Command
	xmlExtensions map[string]xmlExtension
	xmlFeatures   map[string]CoreVersion
)

func parseXML() map[string]any {
	result := map[string]any{
		"types": xmlTypes{
			handles: map[string]Handle{},
			structs: map[string]Struct{},
			unions:  map[string]Union{},
		},
		"enums":   xmlEnums{},
		"feature": xmlFeatures{},
	}
	var decoder *xml.Decoder
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
		started := 0
		for {
			if err != nil {
				return xml.EndElement{}, err
			}
			if _, ok := t.(xml.StartElement); ok {
				started += 1
			}
			if end, ok := t.(xml.EndElement); ok {
				if started > 0 {
					started -= 1
				} else {
					return end, nil
				}
			}
			t, err = decoder.Token()
		}
	}
	findAttribute := func(attrs []xml.Attr, names ...string) xml.Attr {
		for _, name := range names {
			for _, a := range attrs {
				if a.Name.Local == name {
					return a
				}
			}
		}
		return xml.Attr{}
	}
	line := func() int {
		l, _ := decoder.InputPos()
		return l
	}
	{
		parsers := map[string]func(xmlParserInterface, xml.StartElement, any) any{
			"platforms":  xmlParsePlatforms,
			"types":      xmlParseTypes,
			"commands":   xmlParseCommands,
			"extensions": xmlParseExtensions,
			"enums":      xmlParseEnums,
			"feature":    xmlParseFeature,
		}
		docs := filepath.Join(cgodep.InstallDir("vulkan-docs", toolchain.Target{}, toolchain.BuildRelease), "xml", "vk.xml")
		fIn, err := os.Open(docs)
		if err != nil {
			panic(err)
		}
		decoder = xml.NewDecoder(fIn)
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
					line:            line,
					skip:            decoder.Skip,
				}, next, result[next.Name.Local])
			} else {
				decoder.Skip()
			}
		}
	}
	{
		parsers := map[string]func(xmlParserInterface, xml.StartElement, any) any{
			"types": xmlParseTypes,
			"enums": xmlParseEnums,
		}

		docs := filepath.Join(cgodep.InstallDir("vulkan-docs", toolchain.Target{}, toolchain.BuildRelease), "xml", "video.xml")
		fIn, err := os.Open(docs)
		if err != nil {
			panic(err)
		}
		decoder = xml.NewDecoder(fIn)
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
			pi := xmlParserInterface{
				findNextElement: findNextElement,
				findNextString:  findNextString,
				findElementEnd:  findElementEnd,
				findAttribute:   findAttribute,
				line:            line,
				skip:            decoder.Skip,
			}
			if fn, ok := parsers[next.Name.Local]; ok {
				result[next.Name.Local] = fn(pi, next, result[next.Name.Local])
			} else if next.Name.Local == "extensions" {
				result["video"] = xmlParseVideoExtensions(pi, next, result["video"])
			} else {
				decoder.Skip()
			}
		}
	}

	{
		enums := result["enums"].(xmlEnums)
		types := result["types"].(xmlTypes)
		extensions := result["extensions"].(xmlExtensions)
		coreVersions := result["feature"].(xmlFeatures)

		// core versions and extensions in the xml does not contain any of the defined flags,
		// we have to manually add them back after scanning for enums
		addEnumValues := func(e *Exports) {
			for _, t := range e.Types {
				if len(enums[t]) > 0 && !isEnumTypeBlacklisted(t) {
					e.Enums[t] = append(e.Enums[t], enums[t]...)
				}
			}
			e.Types = slices.DeleteFunc(e.Types, func(t string) bool {
				blacklist := map[string]bool{
					"VkBaseInStructure":  true,
					"VkBaseOutStructure": true,
				}
				return blacklist[t]
			})
		}
		filterHandles := func(e *Exports) {
			for _, k := range e.Types {
				// version info does not differentiate between handles and types
				if _, ok := types.handles[k]; ok {
					e.Handles = append(e.Handles, k)
				}
			}
			// remove handles from the type list
			for _, k := range e.Handles {
				e.Types = slices.DeleteFunc(e.Types, func(i string) bool { return i == k })
			}
		}
		for _, v := range coreVersions {
			addEnumValues(&v.Exports)
			filterHandles(&v.Exports)
			coreVersions[v.VersionString] = v
		}
		for _, e := range extensions {
			addEnumValues(&e.Exports)
			filterHandles(&e.Exports)
			extensions[e.Name] = e
		}

		result["feature"] = coreVersions
		result["extensions"] = extensions
	}

	return result
}

func xmlParsePlatforms(vkxml xmlParserInterface, _ xml.StartElement, _ any) any {
	result := xmlPlatforms{}
	for {
		node, err := vkxml.findNextElement()
		if err != nil {
			if errors.Is(err, io.EOF) {
				break
			}
			panic(err)
		}
		if node.Name.Local != "platform" {
			err := vkxml.skip()
			if err != nil {
				if errors.Is(err, io.EOF) {
					break
				}
				panic(err)
			}
			continue
		}
		{
			name := vkxml.findAttribute(node.Attr, "name").Value
			guard := vkxml.findAttribute(node.Attr, "protect").Value
			// the header files are generated in the pattern of "vulkan_<platform>", EXCEPT for provisional,
			// provisional is vulkan_beta.h, fix that
			if name == "provisional" {
				name = "beta"
			}
			result[name] = Platform{
				Name:  name,
				Guard: guard,
			}
		}
		{
			_, err := vkxml.findElementEnd()
			if err != nil {
				panic(err)
			}
		}
	}
	return result
}

func xmlParseTypes(vkxml xmlParserInterface, _ xml.StartElement, result any) any {
	types := result.(xmlTypes)
	type xmlTypeParser struct {
		fn   func(xmlParserInterface, xml.StartElement, any)
		data any
	}
	parsers := map[string]xmlTypeParser{
		"handle": {
			fn:   xmlParseHandles,
			data: types.handles,
		},
		"struct": {
			fn:   xmlParseStructs,
			data: types.structs,
		},
		"union": {
			fn:   xmlParseUnions,
			data: types.unions,
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
			category := vkxml.findAttribute(start.Attr, "category")
			p, ok := parsers[category.Value]
			if ok {
				p.fn(vkxml, start, p.data)
			} else {
				err := vkxml.skip()
				if err != nil {
					panic(err)
				}
			}
		}
	}

	for k, t := range types.handles {
		if t.Alias != "" {
			alias := types.handles[t.Alias]
			for alias.Alias != "" {
				alias = types.handles[alias.Alias]
			}
			if alias.Name == "" {
				panic(fmt.Sprintf("Type %q marked as alias but %q not found", t.Name, t.Alias))
			}
			t.Parent = alias.Parent
			t.Declaration = alias.Declaration
			types.handles[k] = t
		}
	}

	for _, s := range types.structs {
		if s.Alias != "" {
			alias := types.structs[s.Alias]
			for alias.Alias != "" {
				alias = types.structs[alias.Alias]
			}
			if alias.Name == "" {
				panic(fmt.Sprintf("Struct %q marked as alias but %q not found", s.Name, s.Alias))
			}
			s.Params = alias.Params
			types.structs[s.Name] = s
		}
	}
	return types
}

func xmlParseHandles(vkxml xmlParserInterface, start xml.StartElement, result any) {
	handles := result.(map[string]Handle)
	if vkxml.findAttribute(start.Attr, "objtypeenum").Value == "" {
		name := vkxml.findAttribute(start.Attr, "name").Value
		parent := vkxml.findAttribute(start.Attr, "alias").Value
		if name != "" {
			handles[name] = Handle{
				Name:  name,
				Alias: parent,
			}
		}
		{
			_, err := vkxml.findElementEnd()
			if err != nil {
				panic(err)
			}
		}
		return
	}

	var kind, name string
	parent := vkxml.findAttribute(start.Attr, "parent").Value
	objtype := vkxml.findAttribute(start.Attr, "objtypeenum").Value

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
	{
		_, err := vkxml.findElementEnd()
		if err != nil {
			panic(err)
		}
	}

	if name == "" {
		panic("Unexpected empty name")
	}

	handles[name] = Handle{
		Name:        name,
		ObjectType:  objtype,
		Parent:      parent,
		Declaration: kind + "(" + name + ")",
	}
}

func xmlParseParam(vkxml xmlParserInterface, entryName string) []Param {
	var params []Param
	for {
		p := Param{}
		param, err := vkxml.findNextElement()
		if err != nil {
			if errors.Is(err, io.EOF) {
				break
			}
			panic(err)
		}
		if param.Name.Local != entryName {
			if err := vkxml.skip(); err != nil {
				panic(err)
			}
			continue
		}
		{
			api := vkxml.findAttribute(param.Attr, "api").Value
			if api != "" && !slices.Contains(strings.Split(api, ","), "vulkan") {
				if err := vkxml.skip(); err != nil {
					panic(err)
				}
				continue
			}
		}
		{
			p.PairedVar = vkxml.findAttribute(param.Attr, "altlen", "len", "selector").Value
			p.AllowedValues = vkxml.findAttribute(param.Attr, "values", "deprecated").Value
			if p.AllowedValues == "ignored" {
				p.AllowedValues = "unused"
			}
			constStr, err := vkxml.findNextString()
			if err != nil && err != io.EOF {
				panic(err)
			}
			if strings.TrimSpace(constStr) == "const" {
				p.ReadOnly = true
			}
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
				ptrStr = strings.Map(func(r rune) rune {
					if unicode.IsSpace(r) {
						return -1
					}
					return r
				}, ptrStr)
				switch ptrStr {
				case "*":
					p.Kind = ParamKindPointer
				case "**":
					p.Kind = ParamKindPointerToPointer
				case "*const*":
					p.Kind = ParamKindPointerToPointer
					p.ReadOnly = true

				default:
					if p.PairedVar != "" && p.PairedVar != "null-terminated" {
						p.Kind = ParamKindUnion
					} else if ptrStr != "" {
						panic("unhandled: " + ptrStr)
					}
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
			{
				if varSize, err := vkxml.findNextString(); err == nil {
					if strings.HasPrefix(varSize, "[") && strings.HasSuffix(varSize, "]") {
						if p.Kind == ParamKindValue || p.Kind == ParamKindUnion {
							if p.PairedVar == "" {
								p.Kind = ParamKindFixedArray
								p.PairedVar = strings.ReplaceAll(strings.Trim(strings.TrimSpace(varSize), "[]"), "][", ",")
							} else {
								p.Kind = ParamKindFixedArrayVariableCount
								p.AllowedValues = varSize
							}
						}
					}
					if node, err := vkxml.findNextElement(); err == nil {
						if node.Name.Local == "enum" {
							varSize, err := vkxml.findNextString()
							if err != nil {
								panic(err)
							}
							if varSize != "" {
								if p.Kind == ParamKindValue || p.Kind == ParamKindUnion {
									if p.PairedVar == "" {
										p.Kind = ParamKindFixedArray
										p.PairedVar = varSize
									} else {
										p.Kind = ParamKindFixedArrayVariableCount
										p.AllowedValues = varSize
									}
								}
							}
						}
						_, err = vkxml.findElementEnd()
						if err != nil {
							panic(err)
						}
						_, err = vkxml.findElementEnd()
						if err != nil {
							panic(err)
						}
					}
				} else {
					_, err = vkxml.findElementEnd()
					if err != nil {
						panic(err)
					}
				}
			}
			params = append(params, p)
		}
	}
	return params
}

func xmlParseStructs(vkxml xmlParserInterface, start xml.StartElement, result any) {
	structs := result.(map[string]Struct)
	alias := vkxml.findAttribute(start.Attr, "alias")
	name := vkxml.findAttribute(start.Attr, "name")
	if alias.Value != "" {
		structs[name.Value] = Struct{
			Name:  name.Value,
			Alias: alias.Value,
		}
		_, err := vkxml.findElementEnd()
		if err != nil {
			panic(err)
		}
		return
	}
	structs[name.Value] = Struct{
		Name:   name.Value,
		Params: xmlParseParam(vkxml, "member"),
	}
}

func xmlParseUnions(vkxml xmlParserInterface, start xml.StartElement, result any) {
	unions := result.(map[string]Union)
	alias := vkxml.findAttribute(start.Attr, "alias")
	name := vkxml.findAttribute(start.Attr, "name")
	if alias.Value != "" {
		unions[name.Value] = Union{
			Name:  name.Value,
			Alias: alias.Value,
		}
		_, err := vkxml.findElementEnd()
		if err != nil {
			panic(err)
		}
		return
	}
	unions[name.Value] = Union{
		Name:   name.Value,
		Params: xmlParseParam(vkxml, "member"),
	}
}

func xmlParseCommands(vkxml xmlParserInterface, _ xml.StartElement, _ any) any {
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
			export := vkxml.findAttribute(start.Attr, "export")
			if export.Value != "" && !slices.Contains(strings.Split(export.Value, ","), "vulkan") {
				if err := vkxml.skip(); err != nil {
					panic(err)
				}
				continue
			}

			alias := vkxml.findAttribute(start.Attr, "alias")
			if alias.Value != "" {
				name := vkxml.findAttribute(start.Attr, "name")
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

			cmd.Params = xmlParseParam(vkxml, "param")
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
			if alias.Name == "" {
				panic(fmt.Sprintf("CMD %q marked as alias but %q not found", cmd.Name, cmd.Alias))
			}
			cmd.ReturnType = alias.ReturnType
			cmd.Params = alias.Params
			result[cmd.Name] = cmd
		}
	}

	return result
}

func xmlParseExports(vkxml xmlParserInterface, node xml.StartElement, e *Exports) {
	switch node.Name.Local {
	case "enum":
		typename := vkxml.findAttribute(node.Attr, "extends").Value
		if typename != "" && !isTypeBlacklisted(typename) && !isEnumTypeBlacklisted(typename) {
			valuename := vkxml.findAttribute(node.Attr, "name").Value
			t := strings.ReplaceAll(typename, "FlagBits", "Flags")
			i, seen := slices.BinarySearch(e.Enums[t], valuename)
			if !seen {
				e.Enums[t] = slices.Insert(e.Enums[t], i, valuename)
			}
		}
	case "type":
		typename := vkxml.findAttribute(node.Attr, "name").Value
		// we do not want function pointers
		if typename != "" && !strings.HasPrefix(typename, "PFN") &&
			!strings.HasSuffix(typename, ".h") &&
			!isTypeBlacklisted(typename) {
			t := strings.ReplaceAll(typename, "FlagBits", "Flags")
			i, seen := slices.BinarySearch(e.Types, t)
			if !seen {
				e.Types = slices.Insert(e.Types, i, t)
			}
		}
	case "command":
		typename := vkxml.findAttribute(node.Attr, "name").Value
		if typename != "" && !isTypeBlacklisted(typename) {
			i, seen := slices.BinarySearch(e.Commands, typename)
			if !seen {
				e.Commands = slices.Insert(e.Commands, i, typename)
			}
		}
	case "comment":
	default:
		debug.WPrintln("Unhandled element:", node.Name.Local, "line", vkxml.line())
	}
}

func xmlParseExtensions(vkxml xmlParserInterface, _ xml.StartElement, _ any) any {
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
			rootName := vkxml.findAttribute(start.Attr, "name")
			rootKind := vkxml.findAttribute(start.Attr, "type")
			rootDepends := vkxml.findAttribute(start.Attr, "depends")
			rootSupported := vkxml.findAttribute(start.Attr, "supported")
			rootPromoted := vkxml.findAttribute(start.Attr, "promotedto")
			rootDeprecated := vkxml.findAttribute(start.Attr, "deprecatedby")
			rootProvisional := vkxml.findAttribute(start.Attr, "provisional")
			rootPlatform := vkxml.findAttribute(start.Attr, "platform")
			rootNoFeatures := vkxml.findAttribute(start.Attr, "nofeatures")

			if !slices.Contains(strings.Split(rootSupported.Value, ","), "vulkan") {
				if err := vkxml.skip(); err != nil {
					panic(err)
				}
				continue
			}

			e := xmlExtension{
				Extension: Extension{
					Name:     rootName.Value,
					Kind:     rootKind.Value,
					Platform: rootPlatform.Value,
					Promoted: rootPromoted.Value,
					Depends:  rootDepends.Value,
					Exports: Exports{
						Enums: map[string][]string{},
					},
				},
				DependentExports: map[string]Exports{},
			}
			if rootProvisional.Value == "true" {
				if e.Platform != "provisional" {
					panic(fmt.Sprintf("%q has unexpected platform=%q with provisional=true", e.Name, e.Platform))
				}
				// the header files are generated in the pattern of "vulkan_<platform>", EXCEPT for provisional,
				// provisional is vulkan_beta.h, fix that
				e.Platform = "beta"
			}
			if strings.Contains(e.Platform, "provisional") {
				panic(fmt.Sprintf("%q has unexpected platform=%q without provisional=true", e.Name, e.Platform))
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
					if api := vkxml.findAttribute(node.Attr, "api").Value; api != "" {
						switch api {
						case "vulkan":
						case "vulkansc":
							vkxml.skip()
							continue
						default:
							panic(fmt.Sprintf("Unexpected extension api dependency: %q requires %q", e.Name, api))
						}
					}
					depends := vkxml.findAttribute(node.Attr, "depends").Value
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
						case "feature":
							typename := vkxml.findAttribute(t.Attr, "struct").Value
							_, isExtensionType := slices.BinarySearch(e.Exports.Types, typename)
							if isExtensionType {
								if e.FeatureStruct != "" && e.FeatureStruct != typename {
									panic(fmt.Sprintf("Unexpected multiple feature structs %q and %q in %q",
										e.FeatureStruct, typename, e.Name))
								}
								e.FeatureStruct = typename
							}
						default:
							if depends == "" {
								xmlParseExports(vkxml, t, &e.Exports)
							} else {
								exports := e.DependentExports[depends]
								if exports.Enums == nil {
									exports = Exports{
										Enums: map[string][]string{},
									}
								}
								xmlParseExports(vkxml, t, &exports)
								e.DependentExports[depends] = exports
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
			// extension info manual overrides
			switch e.Name {
			case "VK_EXT_subgroup_size_control":
				e.FeatureStruct = "VkPhysicalDeviceSubgroupSizeControlFeaturesEXT"
			case "VK_EXT_pipeline_creation_cache_control":
				e.FeatureStruct = "VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT"
			case "VK_EXT_extended_dynamic_state3":
				e.FeatureStruct = "VkPhysicalDeviceExtendedDynamicState3FeaturesEXT"
			case "VK_KHR_variable_pointers":
				e.FeatureStruct = "VkPhysicalDeviceVariablePointersFeaturesKHR"
			}
			if rootNoFeatures.Value != "true" && e.FeatureStruct == "" {
				panic(fmt.Sprintf("Unable to find feature struct for %q\n", e.Name))
			}
			if rootNoFeatures.Value == "true" && e.FeatureStruct != "" {
				panic(fmt.Sprintf("Incorrectly found feature struct for %q\n", e.Name))
			}
			extensions[e.Name] = e
		}
	}

	return extensions
}

func xmlParseVideoExports(vkxml xmlParserInterface, node xml.StartElement, e *Exports) {
	switch node.Name.Local {
	case "enum":
		typename := vkxml.findAttribute(node.Attr, "extends").Value
		if typename != "" && !isTypeBlacklisted(typename) && !isEnumTypeBlacklisted(typename) {
			valuename := vkxml.findAttribute(node.Attr, "name").Value
			t := strings.ReplaceAll(typename, "FlagBits", "Flags")
			i, seen := slices.BinarySearch(e.Enums[t], valuename)
			if !seen {
				e.Enums[t] = slices.Insert(e.Enums[t], i, valuename)
			}
		}
	case "type":
		typename := vkxml.findAttribute(node.Attr, "name").Value
		// we do not want function pointers
		if typename != "" && !strings.HasPrefix(typename, "PFN") &&
			!strings.HasSuffix(typename, ".h") &&
			!isTypeBlacklisted(typename) {
			t := strings.ReplaceAll(typename, "FlagBits", "Flags")
			i, seen := slices.BinarySearch(e.Types, t)
			if !seen {
				e.Types = slices.Insert(e.Types, i, t)
			}
		}
	case "comment":
	default:
		debug.WPrintln("Unhandled element:", node.Name.Local, "line", vkxml.line())
	}
}

func xmlParseVideoExtensions(vkxml xmlParserInterface, _ xml.StartElement, _ any) any {
	exports := Exports{
		Enums: map[string][]string{},
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
			rootSupported := vkxml.findAttribute(start.Attr, "supported")
			if !slices.Contains(strings.Split(rootSupported.Value, ","), "vulkan") {
				if err := vkxml.skip(); err != nil {
					panic(err)
				}
				continue
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
						xmlParseVideoExports(vkxml, t, &exports)
					}

				default:
					err = vkxml.skip()
					if err != nil {
						panic(err)
					}
				}
			}
		}
	}
	return exports
}

func xmlParseEnums(vkxml xmlParserInterface, start xml.StartElement, result any) any {
	enums := result.(xmlEnums)

	rootType := vkxml.findAttribute(start.Attr, "type").Value
	rootName := strings.ReplaceAll(vkxml.findAttribute(start.Attr, "name").Value, "FlagBits", "Flags")

	switch rootType {
	case "enum", "bitmask":
	case "constants":
		rootName = "constants"
	default:
		if err := vkxml.skip(); err != nil {
			panic(err)
		}
		return enums
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
		case "enum":
			nodeName := vkxml.findAttribute(node.Attr, "name")
			enums[rootName] = append(enums[rootName], nodeName.Value)
		}
		_, err = vkxml.findElementEnd()
		if err != nil {
			panic(err)
		}
	}
	return enums
}

func xmlParseFeature(vkxml xmlParserInterface, start xml.StartElement, result any) any {
	coreVersions := result.(xmlFeatures)

	rootAPI := vkxml.findAttribute(start.Attr, "api")
	rootNumber := vkxml.findAttribute(start.Attr, "number")

	if !slices.Contains(strings.Split(rootAPI.Value, ","), "vulkan") {
		if err := vkxml.skip(); err != nil {
			panic(err)
		}
		return coreVersions
	}

	v := coreVersions[rootNumber.Value]
	v.VersionString = rootNumber.Value
	if v.Exports.Enums == nil {
		v.Exports.Enums = map[string][]string{}
	}
	for {
		start, err := vkxml.findNextElement()
		if err != nil {
			if errors.Is(err, io.EOF) {
				break
			}
			panic(err)
		}
		if start.Name.Local != "require" || strings.Contains(vkxml.findAttribute(start.Attr, "comment").Value, "boilerplate") ||
			strings.Contains(vkxml.findAttribute(start.Attr, "comment").Value, "macros") {
			_, err = vkxml.findElementEnd()
			if err != nil {
				panic(err)
			}
			continue
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
			case "feature":
			default:
				xmlParseExports(vkxml, node, &v.Exports)
			}
			_, err = vkxml.findElementEnd()
			if err != nil {
				panic(err)
			}
		}
	}

	coreVersions[v.VersionString] = v
	return coreVersions
}
