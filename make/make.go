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

package vkm

//go:generate go run ./const_gen.go

/*
	#include "../include/vkm/vkm_config.h"
*/
import "C"

import (
	"encoding/json"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"goarrg.com/debug"
	"goarrg.com/lib/vkm/vkspec"
	"goarrg.com/toolchain"
	"goarrg.com/toolchain/cgodep"
	"goarrg.com/toolchain/golang"
)

func installGeneratorDeps() {
	if err := installVkHeaders(); err != nil {
		panic(debug.ErrorWrapf(err, "Failed to install vulkan-headers"))
	}
	if err := installVkDocs(); err != nil {
		panic(debug.ErrorWrapf(err, "Failed to install vulkan-docs"))
	}
}

type EnableFeatures struct {
	PIC bool // If true, will add -fPIC on linux
}
type (
	DisableFeatures struct{}
	BuildOptions    struct {
		Build   toolchain.Build
		Enable  EnableFeatures
		Disable DisableFeatures
	}
)

type Config struct {
	ForceRebuild bool
	Target       toolchain.Target
	BuildOptions BuildOptions
}

func buildTags(b BuildOptions) string {
	var str string

	return strings.TrimSuffix(str, ",")
}

func Install(c Config) string {
	var flags string = "-Wno-dll-attribute-on-redeclaration "
	var ldflags string
	switch c.BuildOptions.Build {
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
	toolchain.EnvRegister("CGO_CFLAGS", flags)
	toolchain.EnvRegister("CGO_CXXFLAGS", flags)
	toolchain.EnvRegister("CGO_LDFLAGS", ldflags)
	installGeneratorDeps()
	if err := installVKM(c); err != nil {
		panic(debug.ErrorWrapf(err, "Failed to install vkm"))
	}
	return buildTags(c.BuildOptions)
}

func scanDirModTime(dir string, ignore []string) time.Time {
	latestMod := time.Unix(0, 0)

	err := filepath.Walk(dir, func(path string, fs fs.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if path == dir {
			return err
		}
		{
			rel := strings.TrimPrefix(path, dir+string(filepath.Separator))
			for _, i := range ignore {
				if rel == i {
					if fs.IsDir() {
						return filepath.SkipDir
					}
					return err
				}
			}
		}
		mod := fs.ModTime()
		if mod.After(latestMod) {
			latestMod = mod
		}
		return err
	})
	if err != nil {
		panic(err)
	}

	return latestMod
}

func processSrc(c Config, rootDir string, p func(string, []string) error) ([]string, []string, error) {
	srcDir := filepath.Join(rootDir, "src")

	deps := []string{"vulkan-headers"}
	cFlags, err := cgodep.Resolve(c.Target, cgodep.ResolveCFlags, deps...)
	if err != nil {
		return nil, nil, err
	}
	ldFlags, err := cgodep.Resolve(c.Target, cgodep.ResolveLDFlags, deps...)
	if err != nil {
		return nil, nil, err
	}

	flags := append(cFlags, "-I"+srcDir, "-Werror=vla", "-Wall", "-Wextra", "-Wpedantic",
		"-Wno-unknown-pragmas", "-Wno-missing-field-initializers", "-Wno-format-security",
	)

	return cFlags, ldFlags, filepath.WalkDir(srcDir, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}

		switch filepath.Ext(path) {
		case ".h", ".c":
			args := append([]string(nil), flags...)
			args = append(args, strings.Split(toolchain.EnvGet("CGO_CFLAGS"), " ")...)
			args = append(args,
				"-std=c17",
			)
			return p(path, args)

		case ".hpp", ".cpp":
			args := append([]string(nil), flags...)
			args = append(args, strings.Split(toolchain.EnvGet("CGO_CXXFLAGS"), " ")...)
			args = append(args,
				"-std=c++20",
				//"-ffile-prefix-map=" + srcDir + "=src",
			)
			return p(path, args)

		default:
		}

		return nil
	})
}

func installVKM(c Config) error {
	vkapi := uint64(C.VKM_VK_MIN_API)
	module := golang.CallersModule()
	srcDir := module.Dir
	srcVersion := scanDirModTime(filepath.Join(module.Dir, "src"), []string{".cache", ".clang-tidy", ".clangd", "inc", "compile_commands.json"})
	makeVersion := scanDirModTime(filepath.Join(module.Dir, "make"), nil)

	{
		headerVersion := scanDirModTime(filepath.Join(module.Dir, "include"), []string{".cache", ".clang-tidy", ".clangd", "inc", "compile_commands.json"})
		if headerVersion.Unix() > srcVersion.Unix() {
			srcVersion = headerVersion
		}
	}

	version := strconv.FormatInt(srcVersion.Unix(), 16) + "-" + strconv.FormatInt(makeVersion.Unix(), 16)
	{
		vkInstallDir := cgodep.InstallDir("vulkan-headers", toolchain.Target{}, toolchain.BuildRelease)
		vkVersion := cgodep.ReadVersion(vkInstallDir)
		if vkVersion == "" {
			return debug.Errorf("Vulkan-headers not found")
		}
		version += "-vk" + vkVersion[:strings.Index(vkVersion, "-")]
	}
	installDir := cgodep.InstallDir("vkm", c.Target, c.BuildOptions.Build)
	includeDir := filepath.Join(installDir, "include")
	installedVersion := cgodep.ReadVersion(installDir)
	if !c.ForceRebuild && installedVersion == version {
		return cgodep.SetActiveBuild("vkm", c.Target, c.BuildOptions.Build)
	}

	if err := os.RemoveAll(installDir); err != nil {
		return err
	}

	{
		installGeneratorDeps()
		genNonShipables(srcDir, filepath.Join(includeDir, "vkm", "inc"), vkspec.Parse(), vkapi)
		err := filepath.WalkDir(filepath.Join(srcDir, "include"), func(path string, d os.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if d.IsDir() {
				if filepath.Dir(path) == "inc" {
					return filepath.SkipDir
				}
				return nil
			}
			newPath := filepath.Join(includeDir, strings.TrimPrefix(path, filepath.Join(srcDir, "include")))
			switch filepath.Ext(path) {
			case ".h", ".hpp":
			default:
				return nil
			}
			err = os.MkdirAll(filepath.Dir(newPath), 0o755)
			if err != nil {
				panic(err)
			}
			data, err := os.ReadFile(path)
			if err != nil {
				panic(err)
			}
			err = os.WriteFile(newPath, data, 0o644)
			if err != nil {
				panic(err)
			}
			return nil
		})
		if err != nil {
			panic(err)
		}
	}

	type cmd struct {
		Directory string   `json:"directory"`
		Arguments []string `json:"arguments"`
		File      string   `json:"file"`
	}
	var cmds []cmd
	var compileCmds []cmd
	var cFlags, ldFlags []string

	{
		objs := []string{}
		buildDir, err := os.MkdirTemp("", "vkm")
		if err != nil {
			return err
		}
		defer os.RemoveAll(buildDir)

		cFlags, ldFlags, err = processSrc(c, srcDir, func(path string, args []string) error {
			path = strings.TrimPrefix(path, srcDir+string(filepath.Separator))
			if c.BuildOptions.Enable.PIC && c.Target.OS == "linux" {
				args = append(args, "-fPIC")
			}
			switch filepath.Ext(path) {
			case ".c":
				objs = append(objs, path+".o")
				cmdArgs := append([]string{"-I" + filepath.Join(srcDir, "include")}, args...)
				cmds = append(cmds,
					cmd{Directory: srcDir, Arguments: append([]string{os.Getenv("CC")}, cmdArgs...), File: path})
				compileArgs := append([]string{"-I" + includeDir}, args...)
				compileCmds = append(compileCmds, cmd{Directory: srcDir, Arguments: append([]string{os.Getenv("CC")}, compileArgs...), File: path})
			case ".cpp":
				objs = append(objs, path+".o")
				cmdArgs := append([]string{"-I" + filepath.Join(srcDir, "include")}, args...)
				cmds = append(cmds,
					cmd{Directory: srcDir, Arguments: append([]string{os.Getenv("CXX")}, cmdArgs...), File: path})
				compileArgs := append([]string{"-I" + includeDir}, args...)
				compileCmds = append(compileCmds, cmd{Directory: srcDir, Arguments: append([]string{os.Getenv("CXX")}, compileArgs...), File: path})

			case ".h":
				cmdArgs := append([]string{"-I" + filepath.Join(srcDir, "include")}, args...)
				cmds = append(cmds,
					cmd{Directory: srcDir, Arguments: append([]string{os.Getenv("CC")}, cmdArgs...), File: path})
			case ".hpp":
				cmdArgs := append([]string{"-I" + filepath.Join(srcDir, "include")}, args...)
				cmds = append(cmds,
					cmd{Directory: srcDir, Arguments: append([]string{os.Getenv("CXX")}, cmdArgs...), File: path})
			}
			return nil
		})
		if err != nil {
			return err
		}

		if fOut, err := os.Create(filepath.Join(srcDir, "compile_commands.json")); err == nil {
			defer fOut.Close()
			enc := json.NewEncoder(fOut)
			err = enc.Encode(cmds)
			if err != nil {
				return err
			}
		} else if !strings.Contains(srcDir, filepath.Join("pkg", "mod")) {
			// do not warn if obtained through "go get" as the dir is read only
			debug.WPrintf("Failed to write compile_commands.json")
		}

		ok := atomic.Bool{}
		ok.Store(true)
		wg := sync.WaitGroup{}
		for i, c := range compileCmds {
			obj := filepath.Join(buildDir, objs[i])
			if err := os.MkdirAll(filepath.Dir(obj), 0o755); err != nil {
				return err
			}
			wg.Add(1)
			go func() {
				defer wg.Done()
				if out, err := toolchain.RunCombinedOutput(c.Arguments[0], append(c.Arguments[1:], "-o", obj, "-c", filepath.Join(c.Directory, c.File))...); err != nil {
					debug.EPrintf("%s", out)
					ok.Store(false)
				}
			}()
		}
		wg.Wait()
		if !ok.Load() {
			os.Exit(1)
		}
		if err := os.MkdirAll(filepath.Join(installDir, "lib"), 0o755); err != nil {
			return err
		}
		args := []string{"rcs", filepath.Join(installDir, "lib", "libvkm.a")}
		if err := toolchain.RunDir(buildDir, os.Getenv("AR"), append(args, objs...)...); err != nil {
			return err
		}
	}

	{
		fConfig, err := os.OpenFile(filepath.Join(includeDir, "vkm", "vkm_config.h"), os.O_WRONLY|os.O_APPEND, 0o655)
		if err != nil {
			panic(err)
		}
		defer fConfig.Close()
		_, err = fmt.Fprintf(fConfig, "#define VKM_VK_API %d\n", vkapi)
		if err != nil {
			panic(err)
		}
	}

	golang.SetShouldCleanCache()
	return cgodep.WriteMetaFile("vkm", c.Target, c.BuildOptions.Build, cgodep.Meta{
		Version: version,
		Flags: cgodep.Flags{
			CFlags:        append([]string{"-I" + includeDir}, cFlags...),
			LDFlags:       append([]string{"-L" + filepath.Join(installDir, "lib"), "-lvkm"}, ldFlags...),
			StaticLDFlags: append([]string{"-L" + filepath.Join(installDir, "lib"), "-lvkm"}, ldFlags...),
		},
	})
}

func Lint() error {
	module := golang.CallersModule()
	srcDir := module.Dir
	{
		wg := sync.WaitGroup{}
		_, _, err := processSrc(Config{Target: toolchain.Target{OS: os.Getenv("GOOS"), Arch: os.Getenv("GOARCH")}}, srcDir, func(path string, args []string) error {
			wg.Add(1)
			go func() {
				defer wg.Done()
				path = strings.TrimPrefix(path, srcDir+string(filepath.Separator))
				if strings.HasSuffix(path, "vk_mem_alloc.h") {
					return
				}
				cmdArgs := append([]string{"-warnings-as-errors=*", path, "--", "-I" + filepath.Join(srcDir, "include")}, args...)
				if out, err := toolchain.RunDirCombinedOutput(srcDir, "clang-tidy", cmdArgs...); err != nil {
					debug.EPrintf("%s", out)
					os.Exit(1)
				}
			}()
			return nil
		})
		wg.Wait()
		if err != nil {
			return err
		}
	}
	debug.IPrintf("Linting NDEBUG")
	{
		wg := sync.WaitGroup{}
		_, _, err := processSrc(Config{Target: toolchain.Target{OS: os.Getenv("GOOS"), Arch: os.Getenv("GOARCH")}}, srcDir, func(path string, args []string) error {
			wg.Add(1)
			go func() {
				defer wg.Done()
				path = strings.TrimPrefix(path, srcDir+string(filepath.Separator))
				if strings.HasSuffix(path, "vk_mem_alloc.h") {
					return
				}
				cmdArgs := append([]string{"-warnings-as-errors=*", path, "--", "-I" + filepath.Join(srcDir, "include"), "-DNDEBUG"}, args...)
				if out, err := toolchain.RunDirCombinedOutput(srcDir, "clang-tidy", cmdArgs...); err != nil {
					debug.EPrintf("%s", out)
					os.Exit(1)
				}
			}()
			return nil
		})
		wg.Wait()
		if err != nil {
			return err
		}
	}
	return nil
}
