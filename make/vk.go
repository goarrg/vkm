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

import (
	"path/filepath"
	"strings"

	"goarrg.com/toolchain"
	"goarrg.com/toolchain/cgodep"
	"goarrg.com/toolchain/git"
	"goarrg.com/toolchain/golang"
)

const (
	vkHeadersURL   = "https://github.com/KhronosGroup/Vulkan-Headers.git"
	vkHeadersBuild = "-goarrg0"

	vkDocsURL   = "https://github.com/KhronosGroup/Vulkan-Docs.git"
	vkDocsBuild = "-goarrg0"
)

func installVkHeaders() error {
	installDir := cgodep.InstallDir("vulkan-headers", toolchain.Target{}, toolchain.BuildRelease)
	var ref git.Ref
	{
		refs, err := git.GetRemoteHeads(vkHeadersURL, "vulkan-sdk-*")
		if err != nil {
			return err
		}
		ref = refs[0]
	}

	{
		version := cgodep.ReadVersion(installDir)
		// search backwards cause branch names have a "-" in them
		i := strings.LastIndex(version, "-")
		if i > 0 {
			j := strings.LastIndex(version[:i], "-")
			if j > 0 {
				branch := "refs/heads/vulkan-sdk-" + version[1:j]
				hash := version[j+1 : i]
				build := version[i:]

				if branch == ref.Name && hash == ref.Hash && build == vkHeadersBuild {
					return nil
				}
			}
		}
	}

	err := git.CloneOrFetch(vkHeadersURL, installDir, ref)
	if err != nil {
		return err
	}

	golang.SetShouldCleanCache()
	return cgodep.WriteMetaFile("vulkan-headers", toolchain.Target{}, toolchain.BuildRelease, cgodep.Meta{
		Version: "v" + ref.Name[strings.Index(ref.Name, "vulkan-sdk-")+11:] + "-" + ref.Hash + vkHeadersBuild, Flags: cgodep.Flags{
			CFlags: []string{"-I" + filepath.Join(installDir, "include")},
		},
	})
}

func installVkDocs() error {
	installDir := cgodep.InstallDir("vulkan-docs", toolchain.Target{}, toolchain.BuildRelease)
	var ref git.Ref
	{
		version := cgodep.ReadVersion(cgodep.InstallDir("vulkan-headers", toolchain.Target{}, toolchain.BuildRelease))
		refs, err := git.GetRemoteTags(vkDocsURL, version[:strings.Index(version, "-")])
		if err != nil {
			return err
		}
		ref = refs[0]
	}

	{
		version := cgodep.ReadVersion(installDir)
		// search backwards cause branch names have a "-" in them
		i := strings.LastIndex(version, "-")
		if i > 0 {
			j := strings.LastIndex(version[:i], "-")
			if j > 0 {
				branch := version[:j]
				hash := version[j+1 : i]
				build := version[i:]

				if branch == ref.Name && hash == ref.Hash && build == vkDocsBuild {
					return nil
				}
			}
		}
	}

	if err := git.CloneOrFetch(vkDocsURL, installDir, ref); err != nil {
		return err
	}
	return cgodep.WriteMetaFile("vulkan-docs", toolchain.Target{}, toolchain.BuildRelease, cgodep.Meta{
		Version: ref.Name + "-" + ref.Hash + vkDocsBuild,
	})
}
