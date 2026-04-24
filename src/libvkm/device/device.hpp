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

#include "vkm/std/array.hpp"

#include "vkm/vkm.h"
#include "device/sync/sync.hpp"
#include "device/vma/vma.hpp"

#define VKM_UUID_VID_OFFSET 0
#define VKM_UUID_INDEX_TYPE uint16_t
#define VKM_UUID_INDEX_OFFSET 4
#define VKM_UUID_DID_OFFSET 10

namespace vkm::vk::device {
struct instance {
	const VkPhysicalDevice vkPhysicalDevice;  // NOLINT(misc-misplaced-const)
	const VkDevice vkDevice;				  // NOLINT(misc-misplaced-const)
	const bool owned;
	struct {
		bool hasEXTSwapchainMaint1;
	} optionalFeatures;

	vkm::std::array<PFN_vkVoidFunction, VKM_DEVICE_VKFN_COUNT> vkfns;

	syncObjectManager syncObjectManager;
	struct vma vma;

	vkm_device_properties properties;

	instance() noexcept = delete;
	instance(vkm_deviceInitInfo) noexcept;
	~instance() noexcept;

	[[nodiscard]] PFN_vkVoidFunction procAddr(vkm_device_vkfn_id) noexcept;

	[[nodiscard]] vkm_device handle() noexcept { return reinterpret_cast<vkm_device>(this); }
	[[nodiscard]] static instance* fromHandle(vkm_device handle) noexcept {
		return reinterpret_cast<instance*>(handle);
	}
};

vkm::std::array<uint8_t, VK_UUID_SIZE> generateUUID(VkPhysicalDevice, VKM_UUID_INDEX_TYPE) noexcept;
void generateUUID(VkPhysicalDeviceProperties, VKM_UUID_INDEX_TYPE, vkm_device_uuid*) noexcept;
void setupProperties(vkm::vk::device::instance*) noexcept;
void setupVKFNs(vkm::vk::device::instance*) noexcept;
void setupVMA(vkm::vk::device::instance*) noexcept;
void destroyVMA(vkm::vk::device::instance*) noexcept;
void destroySync(vkm::vk::device::instance*) noexcept;
}  // namespace vkm::vk::device

#define VK_PROC_DEVICE(device, FN) ((PFN_##FN)(device)->procAddr(VKM_DEVICE_VKFN_##FN))
#ifndef NDEBUG
#define VK_DEBUG_PROC_DEVICE(device, FN) VK_PROC_DEVICE(device, FN)
#else
#define VK_DEBUG_PROC_DEVICE(device, FN)
#endif

#undef VKM_DEVICE_VKFN
#define VKM_DEVICE_VKFN(device, FN) ((PFN_##FN)(device)->procAddr(VKM_DEVICE_VKFN_##FN))
