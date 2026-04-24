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

#include "vkm/std/string.hpp"
#include "vkm/std/stdlib.hpp"

#include "vkm.hpp"
#include "device/device.hpp"

namespace vkm::vk::device {
void setupVKFNs(vkm::vk::device::instance* device) noexcept {
	bool ok = true;
	vkm::std::stringbuilder<char> error;

#undef VK_PROC_DEVICE
#undef VK_DEBUG_PROC_DEVICE

#define VK_PROC_DEVICE(FN)                                       \
	if (!((PFN_##FN)(device)->procAddr(VKM_DEVICE_VKFN_##FN))) { \
		error.write("\nFailed to find: " #FN);                   \
		ok = false;                                              \
	}

#ifndef NDEBUG
#define VK_DEBUG_PROC_DEVICE(FN) VK_PROC_DEVICE(device, FN)
#endif

#include "device_vkfn.inc"	// IWYU pragma: keep

#undef VK_PROC_DEVICE
#undef VK_DEBUG_PROC_DEVICE

	if (!ok) {
		::vkm::fatal(
			vkm::std::sourceLocation::current(), "[vkfn_device] Failed to find all required functions: %s", error.cStr());
	}
}
}  // namespace vkm::vk::device
