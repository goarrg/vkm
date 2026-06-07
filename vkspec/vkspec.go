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

import (
	"fmt"
	"maps"
	"slices"
	"strconv"
	"strings"
)

type TypeKind uint

const (
	TypeKindBaseType TypeKind = iota
	TypeKindHandle
	TypeKindEnum32
	TypeKindBitFlag32
	TypeKindEnum64
	TypeKindBitFlag64
	TypeKindStruct
	TypeKindUnion
)

type Type struct {
	Name        string
	Alias       string
	Kind        TypeKind
	Declaration []string
	Depends     []string
}

type EnumValue struct {
	Name    string
	Alias   string
	Value   string
	Depends []string
}
type Enum struct {
	Name    string
	Alias   string
	Values  map[string]EnumValue
	Depends []string
}

type Handle struct {
	Name        string
	ObjectType  string
	Alias       string
	Parent      string
	Declaration string
	Depends     []string
}

type ParamKind uint

const (
	ParamKindValue ParamKind = iota
	ParamKindUnion
	ParamKindFixedArray
	ParamKindFixedArrayVariableCount
	ParamKindPointer
	ParamKindPointerToPointer
)

type Param struct {
	TypeName      string
	Kind          ParamKind
	ReadOnly      bool
	VarName       string
	PairedVar     string
	AllowedValues string
}

type Struct struct {
	Name    string
	Alias   string
	Params  []Param
	Depends []string
}

type Union struct {
	Name    string
	Alias   string
	Params  []Param
	Depends []string
}

type Command struct {
	ReturnType string
	Name       string
	Alias      string
	Params     []Param
	Depends    []string
}

type Exports struct {
	Enums    map[string][]string
	Types    []string
	Handles  []string
	Commands []string
}

type CoreVersion struct {
	VersionString string
	Exports       Exports
}

type Platform struct {
	Name  string
	Guard string
}

type Extension struct {
	Name          string
	FeatureStruct string
	Kind          string
	Platform      string
	Promoted      string
	Deprecated    string
	Depends       string
	Exports       Exports
}

type Data struct {
	Types        map[string]Type
	Enums        map[string]Enum
	Handles      map[string]Handle
	Structs      map[string]Struct
	Unions       map[string]Union
	Commands     map[string]Command
	CoreVersions map[string]CoreVersion
	Platforms    map[string]Platform
	Extensions   map[string]Extension
}

type ParseConfig struct {
	// Filters out extensions that were promoted to this version,
	// type/etc... data is preserved unless FilterExtension also returns true.
	// 0 results in no filtering.
	FilterCorePromotion uint32

	// Removes extension and their type/etc.. data for extensions where FilterExtension returns true,
	// Export information of the extension may not be fully filled out during this call.
	FilterExtension func(Extension) bool
}

func Parse(config ParseConfig) Data {
	d := Data{}

	var parsedExtensions xmlExtensions
	{
		xmlData := parseXML()
		types := xmlData["types"].(xmlTypes)
		d.Handles = types.handles
		d.Structs = types.structs
		d.Unions = types.unions

		d.Commands = xmlData["commands"].(xmlCommands)
		d.CoreVersions = xmlData["feature"].(xmlFeatures)
		d.Platforms = xmlData["platforms"].(xmlPlatforms)

		parsedExtensions = xmlData["extensions"].(xmlExtensions)
	}

	// we need to parse xml before the header as we need the list of platforms,
	// then blacklist platforms we cannot find the header of
	var blacklistedPlatforms map[string]bool
	d.Types, blacklistedPlatforms = parseHeader(d.Platforms)

	// type and cmd information is global with no origin identifying information
	// we have to filter them to remove ones from extensions we do not report
	// e.g. vulkansc only extensions
	// this also automatically filters out type information from filtered extensions
	{
		blackListedEnums := map[string]bool{}
		whiteListedEnums := map[string]bool{}

		filteredTypes := map[string]Type{}
		FilteredEnums := map[string]Enum{}
		filteredHandles := map[string]Handle{}
		filteredStructs := map[string]Struct{}
		filteredUnions := map[string]Union{}
		filteredCmds := map[string]Command{}
		filteredExtensions := map[string]Extension{}

		insertDependency := func(k string, depends []string) []string {
			i, found := slices.BinarySearch(depends, k)
			if found {
				return depends
			}
			return slices.Insert(depends, i, k)
		}
		filterExports := func(dependency string, e Exports) {
			for _, k := range e.Types {
				if t, ok := d.Structs[k]; ok {
					t.Depends = insertDependency(dependency, t.Depends)
					d.Structs[k] = t
					filteredStructs[k] = t
				}
				if t, ok := d.Unions[k]; ok {
					t.Depends = insertDependency(dependency, t.Depends)
					d.Unions[k] = t
					filteredUnions[k] = t
				}
				if t, ok := d.Types[k]; ok {
					t.Depends = insertDependency(dependency, t.Depends)
					d.Types[k] = t
					filteredTypes[k] = t

					if t.Kind > TypeKindHandle && t.Kind < TypeKindStruct {
						enum := FilteredEnums[k]
						enum.Name = t.Name
						enum.Alias = t.Alias
						enum.Depends = insertDependency(dependency, enum.Depends)
						if enum.Values == nil {
							enum.Values = make(map[string]EnumValue, len(t.Declaration))
						}
						mapValueToEnum := map[string]string{}
						for _, line := range t.Declaration {
							parts := strings.Split(line, " ")
							v := enum.Values[parts[0]]
							v.Name = parts[0]
							switch {
							case strings.Contains(parts[0], "MAX_ENUM"):
								continue
							case strings.HasPrefix(parts[2], "VK"):
								v.Alias = parts[2]
							case mapValueToEnum[parts[2]] != "":
								v.Value = parts[2]
								v.Alias = mapValueToEnum[parts[2]]
							default:
								v.Value = parts[2]
							}
							if v.Alias == "" && mapValueToEnum[v.Name] == "" {
								mapValueToEnum[v.Value] = v.Name
							}
							enum.Values[v.Name] = v
						}
						for k, v := range enum.Values {
							if v.Alias != "" && v.Value == "" {
								v.Value = enum.Values[v.Alias].Value
								enum.Values[k] = v
							}
						}
						FilteredEnums[k] = enum
					}
				} else if strings.HasPrefix(k, "Vk") {
					panic(fmt.Sprintf("%s has type %q but not found in type info", dependency, k))
				}
			}
			for _, k := range e.Handles {
				if t, ok := d.Handles[k]; ok {
					t.Depends = insertDependency(dependency, t.Depends)
					d.Handles[k] = t
					filteredHandles[k] = t
				} else if strings.HasPrefix(k, "Vk") {
					panic(fmt.Sprintf("%s has handle %q but not found in handle info", dependency, k))
				}
				if t, ok := d.Types[k]; ok {
					t.Depends = insertDependency(dependency, t.Depends)
					d.Types[k] = t
					filteredTypes[k] = t
				} else if strings.HasPrefix(k, "Vk") {
					panic(fmt.Sprintf("%s has type %q but not found in type info", dependency, k))
				}
			}
			for _, k := range e.Commands {
				cmd := d.Commands[k]
				cmd.Depends = insertDependency(dependency, cmd.Depends)
				d.Commands[k] = cmd
				filteredCmds[k] = cmd
			}
			for k, v := range e.Enums {
				if t, ok := d.Types[k]; ok {
					t.Depends = insertDependency(dependency, t.Depends)
					d.Types[k] = t
					filteredTypes[k] = t

					if FilteredEnums[k].Values == nil {
						enum := FilteredEnums[k]
						enum.Values = map[string]EnumValue{}
						FilteredEnums[k] = enum
					}
					for _, enum := range v {
						{
							e := FilteredEnums[k].Values[enum]
							e.Depends = insertDependency(dependency, e.Depends)
							FilteredEnums[k].Values[enum] = e
						}
						whiteListedEnums[enum] = true
					}
				} else if strings.HasPrefix(k, "Vk") {
					panic(fmt.Sprintf("%s has enum type %q but not found in type info", dependency, k))
				}
			}
		}
		{
			maps.DeleteFunc(parsedExtensions, func(k string, e xmlExtension) bool {
				if blacklistedPlatforms[e.Platform] || (config.FilterExtension != nil && config.FilterExtension(e.Extension)) {
					for _, v := range e.Exports.Enums {
						for _, enum := range v {
							blackListedEnums[enum] = true
						}
					}
					for _, exports := range e.DependentExports {
						for _, v := range exports.Enums {
							for _, enum := range v {
								blackListedEnums[enum] = true
							}
						}
					}
					return true
				}
				return false
			})
			isCorePromoted := func(e Extension) bool {
				if config.FilterCorePromotion == 0 {
					return false
				}
				promoted := e.Promoted
				for promoted != "" && !strings.HasPrefix(promoted, "VK_VERSION_") {
					promoted = parsedExtensions[promoted].Promoted
				}
				if vkVersion, ok := strings.CutPrefix(promoted, "VK_VERSION_"); ok {
					versionPair := strings.Split(vkVersion, "_")
					major, err := strconv.ParseUint(versionPair[0], 10, 0)
					if err != nil {
						panic(err)
					}
					minor, err := strconv.ParseUint(versionPair[1], 10, 0)
					if err != nil {
						panic(err)
					}
					if config.FilterCorePromotion >= uint32((major<<22)|(minor<<12)) {
						return true
					}
				}
				return false
			}
			for k, xmlE := range parsedExtensions {
				e := xmlE.Extension
				isPromoted := isCorePromoted(e)
				{
					if isPromoted {
						// remove feature structs as they serve no purpose, not even for apidump
						e.Exports.Types = slices.DeleteFunc(e.Exports.Types, func(t string) bool {
							return t == e.FeatureStruct
						})
						filterExports("", e.Exports)
					} else {
						filterExports(e.Name, e.Exports)
					}
					for depends, exports := range xmlE.DependentExports {
						if _, ok := parsedExtensions[depends]; !ok {
							for _, v := range exports.Enums {
								for _, enum := range v {
									blackListedEnums[enum] = true
								}
							}
							continue
						}
						if isPromoted {
							if isCorePromoted(parsedExtensions[depends].Extension) {
								filterExports("", exports)
							} else {
								filterExports(depends, exports)
							}
						} else {
							filterExports(e.Name, exports)
							filterExports(depends, exports)
							maps.Insert(e.Exports.Enums, maps.All(exports.Enums))
							e.Exports.Types = append(e.Exports.Types, exports.Types...)
							e.Exports.Handles = append(e.Exports.Handles, exports.Handles...)
							e.Exports.Commands = append(e.Exports.Commands, exports.Commands...)
						}
					}
				}
				if !isPromoted {
					filteredExtensions[k] = e
				}
			}
		}
		for _, v := range d.CoreVersions {
			versionPair := strings.Split(v.VersionString, ".")
			major, err := strconv.ParseUint(versionPair[0], 10, 0)
			if err != nil {
				panic(err)
			}
			minor, err := strconv.ParseUint(versionPair[1], 10, 0)
			if err != nil {
				panic(err)
			}
			if config.FilterCorePromotion >= uint32((major<<22)|(minor<<12)) {
				filterExports("", v.Exports)
			} else {
				filterExports(v.VersionString, v.Exports)
			}
		}
		// remove enums, likely aliased, from extensions we don't keep
		{
			// blacklist is overzealous, enums can be defined in more than one extension,
			// use whitelist to compensate.
			maps.DeleteFunc(blackListedEnums, func(k string, _ bool) bool {
				return whiteListedEnums[k]
			})
			for k, v := range filteredTypes {
				if v.Kind > TypeKindHandle && v.Kind < TypeKindStruct {
					v.Declaration = slices.DeleteFunc(v.Declaration, func(line string) bool {
						if line == "" {
							return true
						}
						e := strings.TrimSpace(line[:strings.Index(line, "=")])
						return blackListedEnums[e]
					})
				}
				filteredTypes[k] = v
			}
			for _, enum := range FilteredEnums {
				maps.DeleteFunc(enum.Values, func(k string, v EnumValue) bool {
					return v.Name == "" || blackListedEnums[v.Name]
				})
			}
		}
		// remove object types for handles we don't keep
		{
			whiteListed := map[string]bool{
				"VK_OBJECT_TYPE_MAX_ENUM": true,
			}
			for _, h := range filteredHandles {
				whiteListed[h.ObjectType] = true
			}
			v := filteredTypes["VkObjectType"]
			v.Declaration = slices.DeleteFunc(v.Declaration, func(line string) bool {
				e := strings.TrimSpace(line[:strings.Index(line, "=")])
				return !whiteListed[e]
			})
			filteredTypes["VkObjectType"] = v
		}
		// remove stypes types for structs we don't keep
		{
			whiteListed := map[string]bool{
				"VK_STRUCTURE_TYPE_MAX_ENUM": true,
			}
			for _, s := range filteredStructs {
				if s.Params[0].TypeName == "VkStructureType" {
					whiteListed[s.Params[0].AllowedValues] = true
				}
			}
			v := filteredTypes["VkStructureType"]
			v.Declaration = slices.DeleteFunc(v.Declaration, func(line string) bool {
				e := strings.TrimSpace(line[:strings.Index(line, "=")])
				return !whiteListed[e]
			})
			filteredTypes["VkStructureType"] = v
		}
		d.Types = filteredTypes
		d.Enums = FilteredEnums
		d.Handles = filteredHandles
		d.Structs = filteredStructs
		d.Unions = filteredUnions
		d.Commands = filteredCmds
		d.Extensions = filteredExtensions
	}
	return d
}

func isTypeBlacklisted(t string) bool {
	blacklist := map[string]bool{
		"VkFlags":   true,
		"VkFlags64": true,
		"VkPhysicalDeviceBufferAddressFeaturesEXT":    true,
		"VkPhysicalDeviceFeatures2KHR":                true,
		"VkPhysicalDeviceFloat16Int8FeaturesKHR":      true,
		"VkPhysicalDeviceShaderDrawParameterFeatures": true,
		"VkPhysicalDeviceVariablePointerFeatures":     true,
		"VkPhysicalDeviceVariablePointerFeaturesKHR":  true,
	}
	return blacklist[t]
}

func isEnumTypeBlacklisted(t string) bool {
	blacklist := map[string]bool{
		"VkObjectType":    true,
		"VkStructureType": true,
	}
	return blacklist[t]
}
