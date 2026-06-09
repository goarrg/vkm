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
	"os"
	"path/filepath"
	"strings"
	"testing"

	"goarrg.com/toolchain"
)

func TestVkSpec(t *testing.T) {
	files, err := os.ReadDir("testdata")
	if err != nil {
		t.Fatal(err)
	}

	for _, f := range files {
		t.Run(f.Name(), func(t *testing.T) {
			filename := filepath.Join(".", "testdata", f.Name())
			if strings.HasSuffix(filename, ".go") {
				out, err := toolchain.RunCombinedOutput("go", "run", filename)
				if err != nil {
					t.Fatal(string(out))
				}
			}
		})
	}
}
