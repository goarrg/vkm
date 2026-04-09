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
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	"goarrg.com/debug"
	"goarrg.com/lib/vkm/vkspec"
	"goarrg.com/toolchain"
	"goarrg.com/toolchain/cc"
	"goarrg.com/toolchain/cgodep"
	"goarrg.com/toolchain/golang"
)

type EnableFeatures struct {
	PIC bool // If true, will add -fPIC on linux
}
type BuildOptions struct {
	Build  toolchain.Build
	Enable EnableFeatures
}

type Config struct {
	ForceRebuild bool
	ForceStatic  bool
	Target       toolchain.Target
	BuildOptions BuildOptions
}

func Install(c Config) {
	if err := installVKM(c); err != nil {
		panic(debug.ErrorWrapf(err, "Failed to install vkm"))
	}
}

func installVKM(c Config) error {
	vkapi := uint64(C.VKM_VK_MIN_API)
	module := golang.CallersModule()
	srcDir := module.Dir

	var version, buildID string
	{
		makeVersion := toolchain.ScanDirModTime(filepath.Join(module.Dir, "make"), nil)
		var srcVersion time.Time

		{
			blacklist := toolchain.IgnoreBlacklist(".cache", ".clang-tidy", ".clangd", "inc", "compile_commands.json")
			srcVersion = toolchain.ScanDirModTime(filepath.Join(module.Dir, "src"), blacklist)
			headerVersion := toolchain.ScanDirModTime(filepath.Join(module.Dir, "include"), blacklist)
			if headerVersion.Unix() > srcVersion.Unix() {
				srcVersion = headerVersion
			}
		}

		buildID = strconv.FormatInt(srcVersion.Unix(), 16) + "-" + strconv.FormatInt(makeVersion.Unix(), 16)
		if c.BuildOptions.Enable.PIC && c.Target.OS == "linux" {
			buildID += "-fpic"
		} else {
			buildID += "-nofpic"
		}
		version = buildID
		if c.ForceStatic {
			version += "-static"
		}
	}

	installDir := cgodep.InstallDir("vkm", c.Target, c.BuildOptions.Build)
	includeDir := filepath.Join(installDir, "include")
	deps := []string{"vulkan-headers"}

	m := cgodep.Meta{
		Version: version,
		Flags: cgodep.Flags{
			CFlags:        []string{"-I" + includeDir},
			LDFlags:       []string{"-L" + filepath.Join(installDir, "lib"), "-lvkm-shared"},
			StaticLDFlags: []string{"-L" + filepath.Join(installDir, "lib"), "-lvkm-static"},
		},
	}
	if c.ForceStatic {
		m.Flags.LDFlags = m.Flags.StaticLDFlags
	}

	rebuild := c.ForceRebuild
	if !rebuild {
		var err error
		rebuild, err = func() (bool, error) {
			mInstalled, err := cgodep.ReadMetaFile(installDir)
			if err != nil {
				if errors.Is(err, os.ErrNotExist) {
					return true, nil
				}
				return true, err
			}
			ok, err := mInstalled.CompareDependencies(c.Target, deps...)
			if err != nil {
				return true, err
			}
			if !ok {
				return true, nil
			}
			return !strings.HasPrefix(mInstalled.Version, buildID), nil
		}()
		if err != nil {
			return err
		}
	}

	if rebuild {
		if err := os.RemoveAll(installDir); err != nil {
			return err
		}

		{
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

		buildOptions := cc.BuildOptions{
			Type:   cc.BuildTypeStaticLibrary,
			Target: c.Target,
			CompileOnlyFlags: cc.BuildFlags{
				CFlags:   []string{"-I" + includeDir},
				CXXFlags: []string{"-I" + includeDir},
			},
			CommandOnlyFlags: cc.BuildFlags{
				CFlags:   []string{"-I" + filepath.Join(srcDir, "include"), "-Wall", "-Wextra", "-Wpedantic"},
				CXXFlags: []string{"-I" + filepath.Join(srcDir, "include"), "-Wall", "-Wextra", "-Wpedantic"},
			},
		}
		{
			cFlags, err := cgodep.Resolve(c.Target, cgodep.ResolveCFlags, deps...)
			if err != nil {
				return err
			}
			buildFlags := cc.BuildFlags{
				CFlags:   append(cFlags, strings.Split(toolchain.EnvGet("CGO_CFLAGS"), " ")...),
				CXXFlags: append(cFlags, strings.Split(toolchain.EnvGet("CGO_CXXFLAGS"), " ")...),
			}
			{
				extraFlags := []string{"-I" + filepath.Join(srcDir, "src"), "-Werror=vla", "-Wno-unknown-pragmas", "-Wno-missing-field-initializers", "-Wno-format-security"}
				buildFlags.CFlags = append(buildFlags.CFlags, extraFlags...)
				buildFlags.CXXFlags = append(buildFlags.CXXFlags, extraFlags...)
			}
			buildFlags.CFlags = append(buildFlags.CFlags,
				"-std=c17",
			)
			buildFlags.CXXFlags = append(buildFlags.CXXFlags,
				"-std=c++20",
			)
			if c.BuildOptions.Enable.PIC && c.Target.OS == "linux" {
				buildFlags.CFlags = append(buildFlags.CFlags, "-fPIC")
				buildFlags.CXXFlags = append(buildFlags.CXXFlags, "-fPIC")
			}
			buildOptions.BuildFlags = buildFlags

			ldFlags, err := cgodep.Resolve(c.Target, cgodep.ResolveLDFlags, deps...)
			if err != nil {
				return err
			}
			buildOptions.LDFlags = append(ldFlags, strings.Split(toolchain.EnvGet("CGO_LDFLAGS"), " ")...)
		}
		{
			buildDir, err := os.MkdirTemp("", "vkm")
			if err != nil {
				return err
			}
			defer os.RemoveAll(buildDir)

			jsonCmds, err := cc.BuildDir(srcDir, buildDir,
				filepath.Join(installDir, "lib", "libvkm-static"+buildOptions.Type.FileExt(c.Target.OS)), buildOptions)
			if err != nil {
				return err
			}
			{
				jOut, err := json.Marshal(jsonCmds)
				if err != nil {
					return err
				}
				if err := os.WriteFile(filepath.Join(srcDir, "compile_commands.json"), jOut, 0o655); err != nil &&
					!strings.Contains(srcDir, filepath.Join("pkg", "mod")) {
					// do not warn if obtained through "go get" as the dir is read only
					debug.WPrintf("Failed to write compile_commands.json: %v", err)
				}
			}
			buildOptions.Type = cc.BuildTypeSharedLibrary
			_, err = cc.BuildDir(srcDir, buildDir,
				filepath.Join(installDir, "lib", "libvkm-shared"+buildOptions.Type.FileExt(c.Target.OS)), buildOptions)
			if err != nil {
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
	}

	return cgodep.WriteMetaFile(installDir, m)
}

func Lint() {
	module := golang.CallersModule()
	srcDir := module.Dir
	{
		wg := sync.WaitGroup{}
		err := filepath.WalkDir(srcDir, func(path string, d os.DirEntry, err error) error {
			switch filepath.Ext(path) {
			case ".c", ".h", ".cpp", ".hpp":
			default:
				return err
			}
			wg.Add(1)
			go func() {
				defer wg.Done()
				if strings.HasSuffix(path, "vk_mem_alloc.h") {
					return
				}
				cmdArgs := []string{"-warnings-as-errors=*", path}
				if out, err := toolchain.RunDirCombinedOutput(srcDir, "clang-tidy", cmdArgs...); err != nil {
					debug.EPrintf("%s", out)
					os.Exit(1)
				}
			}()
			return nil
		})
		wg.Wait()
		if err != nil {
			panic(err)
		}
	}
	debug.IPrintf("Linting NDEBUG")
	{
		wg := sync.WaitGroup{}
		err := filepath.WalkDir(srcDir, func(path string, d os.DirEntry, err error) error {
			switch filepath.Ext(path) {
			case ".c", ".h", ".cpp", ".hpp":
			default:
				return err
			}
			wg.Add(1)
			go func() {
				defer wg.Done()
				if strings.HasSuffix(path, "vk_mem_alloc.h") {
					return
				}
				cmdArgs := []string{"-warnings-as-errors=*", "--extra-arg=-DNDEBUG", path}
				if out, err := toolchain.RunDirCombinedOutput(srcDir, "clang-tidy", cmdArgs...); err != nil {
					debug.EPrintf("%s", out)
					os.Exit(1)
				}
			}()
			return nil
		})
		wg.Wait()
		if err != nil {
			panic(err)
		}
	}
}
