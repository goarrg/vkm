/*
Copyright 2026 The goARRG Authors.

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

type Type struct {
	Name        string
	Alias       string
	Declaration []string
}

type Handle struct {
	Name     string
	TypeName string
	Alias    string
	Parent   string
}

type CommandParam struct {
	TypeName   string
	IsReadOnly bool
	IsPointer  bool
	VarName    string
}

type Command struct {
	ReturnType string
	Name       string
	Alias      string
	Params     []CommandParam
}

type Extension struct {
	Name        string
	Kind        string
	Platform    string
	Promoted    string
	Deprecated  string
	Valid       bool
	Provisional bool
	Depends     []string
	Types       []string
	Handles     []string
	Commands    []string
	Extends     map[string][]string
}

type Data struct {
	Types      map[string]Type
	Handles    map[string]Handle
	Commands   map[string]Command
	Extensions map[string]Extension
}

func Parse() Data {
	d := Data{
		Types: parseHeader(),
	}
	xmlData := parseXML()
	d.Handles = xmlData["types"].(xmlTypes).handles
	d.Commands = xmlData["commands"].(xmlCommands)
	d.Extensions = xmlData["extensions"].(xmlExtensions)
	return d
}
