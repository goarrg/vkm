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
	"fmt"
	"os"
	"path/filepath"
	"slices"
	"strings"

	"goarrg.com/toolchain"
	"goarrg.com/toolchain/cgodep"
)

func parseHeader() map[string]Type {
	header := filepath.Join(cgodep.InstallDir("vulkan-headers", toolchain.Target{}, toolchain.BuildRelease), "include", "vulkan", "vulkan_core.h")
	fIn, err := os.Open(header)
	if err != nil {
		panic(err)
	}

	scanner := bufio.NewScanner(fIn)
	lastType := ""
	types := map[string]Type{}

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
			if !strings.Contains(line, t) {
				return
			}
			if strings.Contains(line, "=") {
				parts := strings.Split(line, " = ")
				if strings.HasPrefix(parts[1], "0") {
					line = strings.TrimSuffix(line, "ULL")
				}
				t := types[lastType]
				t.Declaration = append(t.Declaration, line)
				types[lastType] = t
			}
		}
	}
	scanFeatureStruct := func() {
		if lastType == "VkPhysicalDeviceFeatures2" {
			for scanner.Scan(); !strings.Contains(scanner.Text(), "}"); scanner.Scan() {
			}
			return
		}
		if lastType != "VkPhysicalDeviceFeatures" {
			t := types[lastType]

			scanner.Scan()
			if !strings.HasPrefix(strings.TrimSpace(scanner.Text()), "VkStructureType") {
				panic(fmt.Sprintf("%s: %s", lastType, scanner.Text()))
			}
			t.Declaration = append(t.Declaration, "VkStructureType sType")
			scanner.Scan()
			if !strings.HasPrefix(strings.TrimSpace(scanner.Text()), "void*") {
				panic(scanner.Text())
			}
			t.Declaration = append(t.Declaration, "void* pNext")

			types[lastType] = t
		}
		for scanner.Scan() {
			str := strings.TrimSuffix(strings.TrimSpace(scanner.Text()), ";")
			if strings.ContainsAny(str, "}") {
				return
			}
			if !strings.HasPrefix(str, "VkBool32") {
				panic(str)
			}
			t := types[lastType]
			t.Declaration = append(t.Declaration, str)
			types[lastType] = t
		}
	}
	scanStruct := func() {
		t := types[lastType]

		scanner.Scan()
		if !strings.HasPrefix(strings.TrimSpace(scanner.Text()), "VkStructureType") {
			for scanner.Scan(); !strings.Contains(scanner.Text(), "}"); scanner.Scan() {
			}
			delete(types, lastType)
			return
		}
		t.Declaration = append(t.Declaration, "VkStructureType sType")
		types[lastType] = t

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
	for scanner.Scan() {
		line := strings.TrimSpace(strings.TrimRight(scanner.Text(), " ,;{\n"))
		{
			t, ok := strings.CutPrefix(line, "typedef enum ")
			if ok {
				lastType = strings.ReplaceAll(t, "FlagBits", "Flags")
				types[lastType] = Type{
					Name: lastType,
				}
				scanEnum()
				continue
			}
		}
		{
			t, ok := strings.CutPrefix(line, "typedef VkFlags64 ")
			if ok && strings.Contains(t, "FlagBits") {
				lastType = strings.ReplaceAll(t, "FlagBits", "Flags")
				types[lastType] = Type{
					Name: lastType,
				}
				scanFlags(t)
				continue
			}
		}
		{
			isAlias := strings.HasPrefix(line, "typedef VkPhysicalDevice")
			isPhysicalDevice := strings.HasPrefix(line, "typedef struct VkPhysicalDevice")
			isFeatures := strings.Contains(line, "Features")
			if isAlias && isFeatures {
				fields := strings.Fields(line)
				// keep list sorted or BinarSearch doesn't work
				blacklist := []string{
					"VkPhysicalDeviceBufferAddressFeaturesEXT",
					"VkPhysicalDeviceFeatures2KHR",
					"VkPhysicalDeviceFloat16Int8FeaturesKHR",
					"VkPhysicalDeviceShaderDrawParameterFeatures",
					"VkPhysicalDeviceVariablePointerFeatures",
					"VkPhysicalDeviceVariablePointerFeaturesKHR",
				}
				if _, skip := slices.BinarySearch(blacklist, fields[2]); !skip {
					types[fields[2]] = Type{
						Name:  fields[2],
						Alias: fields[1],
					}
				}
				continue
			} else if isPhysicalDevice && isFeatures {
				lastType = strings.Fields(line)[2]
				types[lastType] = Type{
					Name: lastType,
				}
				scanFeatureStruct()
				continue
			}
		}
		{
			if t, ok := strings.CutPrefix(line, "typedef struct "); ok {
				lastType = t
				types[t] = Type{
					Name: t,
				}
				scanStruct()
				continue
			}
		}
	}

	for k, t := range types {
		if t.Alias != "" {
			alias := types[t.Alias]
			for alias.Alias != "" {
				alias = types[alias.Alias]
			}
			t.Declaration = alias.Declaration
			types[k] = t
		}
	}

	return types
}
