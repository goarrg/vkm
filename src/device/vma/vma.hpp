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

#pragma once

#ifndef __cplusplus
#error C++ only header
#endif

#include <stdint.h>

// avoids including vulkan.h and thus windows.h
#include "vkm/vkm.h"  // IWYU pragma: keep

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"  // IWYU pragma: export

// VMA version check
// VMA uses non standard version numbers in the format ABBBCCC, where A = major, BBB = minor, CCC = patch.
static_assert(VK_MAKE_API_VERSION(0, VMA_VULKAN_VERSION / 1000000, (VMA_VULKAN_VERSION - 1000000) / 1000, 0) >= VKM_VK_MAX_API);

#pragma GCC diagnostic pop
#pragma clang diagnostic pop

namespace vkm::vk::device {
struct vma {
	VmaAllocator allocator;
	uint32_t noBARMemoryTypeBits;
	uint32_t barMemoryTypeBits;
};
}  // namespace vkm::vk::device
