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
	"bufio"
	"errors"
	"fmt"
	"io"
	"maps"
	"os"
	"path/filepath"
	"slices"
	"strings"

	"goarrg.com/toolchain"
	"goarrg.com/toolchain/cgodep"
)

func parseHeader(platforms map[string]Platform) (map[string]Type, map[string]bool) {
	headerDir := filepath.Join(cgodep.InstallDir("vulkan-headers", toolchain.Target{}, toolchain.BuildRelease), "include")

	types := map[string]Type{}
	blacklistedPlatforms := map[string]bool{}

	scanFile := func(platform, file string) {
		fIn, err := os.Open(file)
		if err != nil {
			if errors.Is(err, os.ErrNotExist) {
				if platform != "" {
					blacklistedPlatforms[platform] = true
				}
				return
			}
			panic(err)
		}
		scanner := bufio.NewScanner(fIn)
		lastType := ""

		scanEnum := func() {
			for scanner.Scan() {
				line := strings.TrimSpace(strings.TrimRight(scanner.Text(), " ,;\n"))
				if strings.Contains(line, "=") {
					parts := strings.Split(line, " = ")
					if strings.HasPrefix(parts[1], "0") {
						line = strings.TrimSuffix(line, "ULL")
					}
					t := types[lastType]
					t.Declaration = append(t.Declaration, line)
					types[lastType] = t
				}
				if strings.Contains(line, "MAX_ENUM") {
					return
				}
			}
		}
		scanFlags := func(t string) {
			for scanner.Scan() {
				line := strings.TrimSpace(strings.TrimRight(scanner.Text(), " ,;\n"))
				if strings.Contains(line, "#") || strings.Contains(line, "//") {
					continue
				}
				// this is based on the assumption that every flags64 ends with a blank line before anything else
				// so far this is true, but may change in the future and this line will cause a whole line to be skipped
				if !strings.Contains(line, t) {
					return
				}
				if strings.Contains(line, "=") {
					parts := strings.Split(line, " = ")
					if strings.HasPrefix(parts[1], "0") {
						line = strings.TrimSuffix(line, "ULL")
					}
					line = strings.TrimPrefix(line, "static const ")
					line = line[strings.Index(line, " ")+1:]
					t := types[lastType]
					t.Declaration = append(t.Declaration, line)
					types[lastType] = t
				}
			}
		}
		scanStruct := func() {
			for scanner.Scan() {
				str := strings.TrimSuffix(strings.TrimSpace(scanner.Text()), ";")
				if strings.ContainsAny(str, "}") {
					return
				}
				t := types[lastType]
				t.Declaration = append(t.Declaration, str)
				types[lastType] = t
			}
		}
		definedMacros := map[string]bool{"VK_ENABLE_BETA_EXTENSIONS": true}
	scan:
		for scanner.Scan() {
			hasSemiColon := strings.Contains(scanner.Text(), ";")
			line := strings.TrimSpace(strings.TrimRight(scanner.Text(), " ,;{\n"))
			if macro, isDef := strings.CutPrefix(line, "#ifdef "); isDef && !definedMacros[strings.TrimSpace(macro)] {
				for scanner.Scan() {
					switch {
					case strings.HasPrefix(scanner.Text(), "#else"),
						strings.HasPrefix(scanner.Text(), "#endif"):
						continue scan
					case strings.HasPrefix(scanner.Text(), "#elif"):
						panic("#elif is unhandled")
					}
				}
			}
			{
				t, ok := strings.CutPrefix(line, "VK_DEFINE_HANDLE(")
				if ok {
					lastType = t[:strings.Index(t, ")")]
					types[lastType] = Type{
						Name:        lastType,
						Kind:        TypeKindHandle,
						Declaration: []string{"VK_DEFINE_HANDLE"},
					}
					continue
				}
			}
			{
				t, ok := strings.CutPrefix(line, "VK_DEFINE_NON_DISPATCHABLE_HANDLE(")
				if ok {
					lastType = t[:strings.Index(t, ")")]
					types[lastType] = Type{
						Name:        lastType,
						Kind:        TypeKindHandle,
						Declaration: []string{"VK_DEFINE_NON_DISPATCHABLE_HANDLE"},
					}
					continue
				}
			}
			{
				t, ok := strings.CutPrefix(line, "typedef enum ")
				if ok {
					kind := TypeKindEnum32
					lastType = strings.ReplaceAll(t, "FlagBits", "Flags")
					if strings.Contains(lastType, "Flags") {
						kind = TypeKindBitFlag32
					}
					lastType = strings.TrimSpace(lastType)
					types[lastType] = Type{
						Name: lastType,
						Kind: kind,
					}
					scanEnum()
					continue
				}
			}
			{
				t, ok := strings.CutPrefix(line, "typedef VkFlags ")
				if ok {
					// VkFlags typenames never contain "bits", that is only the enum
					// VkFlags64 changes that
					lastType = strings.TrimSpace(t)
					// we need to parse flag declarations to account for reserved types,
					// but we also cannot throw out already found bits
					if _, exist := types[lastType]; !exist {
						if strings.Contains(lastType, "Flags") {
							types[lastType] = Type{
								Name: lastType,
								Kind: TypeKindBitFlag32,
							}
						} else {
							types[lastType] = Type{
								Name: lastType,
								Kind: TypeKindEnum32,
							}
						}
					}
					continue
				}
			}
			{
				t, ok := strings.CutPrefix(line, "typedef VkFlags64 ")
				if ok {
					kind := TypeKindEnum64
					lastType = strings.ReplaceAll(t, "FlagBits", "Flags")
					if strings.Contains(lastType, "Flags") {
						kind = TypeKindBitFlag64
					}
					lastType = strings.TrimSpace(lastType)
					if _, exist := types[lastType]; !exist {
						types[lastType] = Type{
							Name: lastType,
							Kind: kind,
						}
					}
					scanFlags(t)
					continue
				}
			}
			{
				if t, ok := strings.CutPrefix(line, "typedef struct "); ok {
					// these are forward declared non vulkan types, skip
					if hasSemiColon {
						continue
					}
					lastType = strings.TrimSpace(t)
					types[t] = Type{
						Name: t,
						Kind: TypeKindStruct,
					}
					scanStruct()
					continue
				}
			}
			{
				if t, ok := strings.CutPrefix(line, "typedef union "); ok {
					lastType = strings.TrimSpace(t)
					types[t] = Type{
						Name: t,
						Kind: TypeKindUnion,
					}
					scanStruct()
					continue
				}
			}
			{
				if t, ok := strings.CutPrefix(line, "typedef "); ok && !strings.ContainsAny(t, "()") {
					fields := strings.Fields(t)
					fields[0] = strings.TrimSpace(strings.ReplaceAll(fields[0], "FlagBits", "Flags"))
					fields[1] = strings.TrimSpace(strings.ReplaceAll(fields[1], "FlagBits", "Flags"))
					if _, exist := types[fields[1]]; !exist {
						if !isTypeBlacklisted(fields[1]) {
							switch {
							case strings.HasPrefix(fields[0], "uint"),
								strings.HasPrefix(fields[0], "void"):
								types[fields[1]] = Type{
									Name:        fields[1],
									Kind:        TypeKindBaseType,
									Declaration: []string{fields[0]},
								}
							default:
								types[fields[1]] = Type{
									Name:  fields[1],
									Alias: fields[0],
								}
							}
						}
					}
				}
			}
		}
		if err := scanner.Err(); err != nil && !errors.Is(err, io.EOF) {
			panic(err)
		}
	}

	// type info is spread out into platform specific files
	{
		// core is the main non platform specific header
		files := append(slices.Collect(maps.Keys(platforms)), "core")
		for _, f := range files {
			scanFile(f, filepath.Join(headerDir, "vulkan", fmt.Sprintf("vulkan_%s.h", f)))
		}
	}
	// video has its own folder for some reason
	{
		files, err := os.ReadDir(filepath.Join(headerDir, "vk_video"))
		if err != nil {
			panic(err)
		}
		for _, f := range files {
			scanFile("", filepath.Join(headerDir, "vk_video", f.Name()))
		}
	}

	for k, t := range types {
		if t.Alias != "" {
			alias := types[t.Alias]
			for alias.Alias != "" {
				alias = types[alias.Alias]
			}
			if alias.Name == "" {
				panic(fmt.Sprintf("Type %q marked as alias but %q not found", t.Name, t.Alias))
			}
			t.Kind = alias.Kind
			t.Declaration = alias.Declaration
			types[k] = t
		}
	}

	return types, blacklistedPlatforms
}
