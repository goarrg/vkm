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

package main

import (
	"fmt"
	"log"
	"slices"

	"goarrg.com/lib/vkm/vkspec"
)

func main() {
	runTest := func(testname string, data vkspec.Data) {
		for _, e := range data.Enums {
			if e.Alias == "" {
				continue
			}
			alias := data.Enums[e.Alias]
			if len(e.Values) != len(alias.Values) {
				log.Fatalf("Inconsistent enum found for %s: %s does not match %s\n%v\n\n%v",
					testname, e.Name, alias.Name, e, alias)
			}
			for _, v := range e.Values {
				if slices.Compare(v.Depends, alias.Values[v.Name].Depends) != 0 {
					log.Fatalf("Inconsistent enum found for %s: %s does not match %s\n%v\n\n%v",
						testname, e.Name, alias.Name, v, alias.Values[v.Name])
				}
			}
		}
	}
	major := 1
	for minor := range 5 {
		testname := fmt.Sprintf("version %d.%d", major, minor)
		runTest(
			testname,
			vkspec.Parse(vkspec.ParseConfig{
				FilterCorePromotion: uint32((major << 22) | (minor << 12)),
			}),
		)
	}
	for minor := range 5 {
		testname := fmt.Sprintf("platform filtered version %d.%d", major, minor)
		runTest(
			testname,
			vkspec.Parse(vkspec.ParseConfig{
				FilterCorePromotion: uint32((major << 22) | (minor << 12)),
				FilterExtension: func(e vkspec.Extension) bool {
					return e.Platform != ""
				},
			}),
		)
	}
}
