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

package main

import (
	"fmt"
	"os"
	"runtime"

	vkm "goarrg.com/lib/vkm/make"
	"goarrg.com/toolchain"
	"goarrg.com/toolchain/cc"
	"goarrg.com/toolchain/cgodep"
	"goarrg.com/toolchain/golang"
)

type command func([]string)

func main() {
	commands := map[string]command{
		"gen":     gen,
		"lint":    lint,
		"install": install,
	}
	if len(os.Args) > 1 {
		if cmd, ok := commands[os.Args[1]]; ok {
			cmd(os.Args[2:])
		} else {
			panic("Unknown Command " + os.Args[1])
		}
	} else {
		install(nil)
	}
}

func gen(args []string) {
	vkm.Gen()
}

func lint(args []string) {
	vkm.Lint()
}

func install(args []string) {
	if len(args) > 1 {
		panic(fmt.Sprintf("Unknown args: %v", args))
	}
	target := toolchain.Target{
		OS:   runtime.GOOS,
		Arch: runtime.GOARCH,
	}
	build := toolchain.BuildRelease

	if len(args) == 1 {
		switch args[0] {
		case "debug":
			build = toolchain.BuildDebug
		case "dev":
			build = toolchain.BuildDevelopment
		case "release":
			build = toolchain.BuildRelease
		default:
			panic("Unknown arg: " + args[0])
		}
	}

	flags := "-Wno-dll-attribute-on-redeclaration "
	ldflags := ""
	switch build {
	case toolchain.BuildRelease:
		flags += "-O2 -march=x86-64-v3 -DNDEBUG"
		ldflags += "-O2"
	case toolchain.BuildDevelopment:
		flags += "-glldb -O2 -march=x86-64-v3"
		ldflags += "-glldb -O2"
	case toolchain.BuildDebug:
		flags += "-glldb -O0 -march=x86-64-v3"
		ldflags += "-glldb -O0"
	}

	os.Setenv("GOAMD64", "v3")
	os.Setenv("CGO_CFLAGS", flags)
	os.Setenv("CGO_CXXFLAGS", flags)
	os.Setenv("CGO_LDFLAGS", ldflags)

	golang.Setup(golang.Config{Target: target})
	cgodep.Install()
	cc.Setup(cc.Config{Compiler: cc.CompilerClang, Target: target})

	vkm.Install(vkm.Config{
		Target: target,
		BuildOptions: vkm.BuildOptions{
			Build: build,
		},
	})

	if golang.ShouldCleanCache() {
		golang.CleanCache()
	}
}
