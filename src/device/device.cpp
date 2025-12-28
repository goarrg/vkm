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

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <new>

#include "vkm/std/array.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/string.hpp"
#include "vkm/std/utility.hpp"

#include "vkm/vkm.h"  // IWYU pragma: associated
#include "vkm.hpp"
#include "reflect_const.hpp"
#include "device/device.hpp"

static_assert(VKM_DEVICE_VKFN_COUNT <= VKM_DEVICE_VKFN_MAX_ENUM);

namespace vkm::vk::device {
instance::instance(vkm_deviceInitInfo info) noexcept
	: vkPhysicalDevice(info.vkPhysicalDevice),
	  vkDevice(info.vkDevice),
	  owned(info.gainOwnership == VK_TRUE),
	  optionalFeatures({.hasEXTSwapchainMaint1 = info.optionalFeatures.extSwapchainMaint1 == VK_TRUE}),
	  syncObjectManager(this) {}
instance::~instance() noexcept {
	static constexpr vkm::std::array deviceDestructors = {
		&destroyVMA,
		&destroySync,
	};
	for (auto destroy : deviceDestructors) {
		destroy(this);
	}
	if (this->owned) {
		VK_PROC_DEVICE(this, vkDestroyDevice)(this->vkDevice, nullptr);
	}
}
// NOLINTNEXTLINE(readability-function-size)
[[nodiscard]] PFN_vkVoidFunction instance::procAddr(vkm_device_vkfn_id fn) noexcept {
	if (this->vkfns[fn] != nullptr) {
		return this->vkfns[fn];
	}

	switch (fn) {
#undef VKM_VKFN
#define VKM_VKFN(FN)                                                                                                     \
	case VKM_DEVICE_VKFN_##FN:                                                                                           \
		this->vkfns[fn] = ((PFN_vkGetDeviceProcAddr)vkm_getProcAddr(VKM_VKFN_vkGetDeviceProcAddr))(this->vkDevice, #FN); \
		return this->vkfns[fn];
#include "vkm/inc/vkfn_dispatch_device.inc"	 // IWYU pragma: keep
#undef VKM_VKFN
#define VKM_VKFN(FN) ((PFN_##FN)vkm_getProcAddr(VKM_VKFN_##FN))

		case VKM_DEVICE_VKFN_COUNT:
		case VKM_DEVICE_VKFN_MAX_ENUM:
			return nullptr;
	}

	return nullptr;
}
vkm::std::array<uint8_t, VK_UUID_SIZE> generateUUID(VkPhysicalDevice target, uint16_t index) noexcept {
	vkm::std::array<uint8_t, VK_UUID_SIZE> uuid;
	VkPhysicalDeviceProperties properties = {};
	VK_PROC(vkGetPhysicalDeviceProperties)(target, &properties);

	vkm::vk::device::generateUUID(properties, index, reinterpret_cast<vkm_device_uuid*>(uuid.get()));
	return vkm::std::move(uuid);
}
void generateUUID(VkPhysicalDeviceProperties properties, VKM_UUID_INDEX_TYPE index, vkm_device_uuid* uuid) noexcept {
	// byte 6 contains UUID version, version 8 means do whatever you want, byte 8 contains variant, F0 is an
	// invalid value we use that as we are not following any known variant
	static constexpr vkm_device_uuid uuidBaseBits = {0, 0, 0, 0, 0, 0, 0x80, 0, 0xF0};
	memcpy(uuid, uuidBaseBits, VK_UUID_SIZE);
	memcpy(uuid + VKM_UUID_VID_OFFSET, &properties.vendorID, sizeof(properties.vendorID));
	// deviceID is not unique enough on multi-gpu systems, throw in the index inot the uuid too
	memcpy(uuid + VKM_UUID_INDEX_OFFSET, &index, sizeof(VKM_UUID_INDEX_TYPE));
	memcpy(uuid + VKM_UUID_DID_OFFSET, &properties.deviceID, sizeof(properties.deviceID));
}
}  // namespace vkm::vk::device

extern "C" {
VKM_FN VkResult vkm_device_getVkPhysicalDeviceFromUUID(vkm_device_uuid wantUUID, VkPhysicalDevice* physicalDevice) {
	uint32_t numDevices = 0;
	VkResult ret = VK_PROC(vkEnumeratePhysicalDevices)(vkm::vkInstance(), &numDevices, nullptr);
	if (ret != VK_SUCCESS) {
		vkm::ePrintf("Failed to get list of GPU devices: %s", vkm::vk::reflect::toString(ret).cStr());
		return ret;
	}
	if (numDevices == 0) {
		vkm::ePrintf("Failed to get list of GPU devices: List is empty");
		return VK_ERROR_INCOMPATIBLE_DRIVER;
	}
	vkm::std::vector<VkPhysicalDevice> devices(numDevices);
	ret = VK_PROC(vkEnumeratePhysicalDevices)(vkm::vkInstance(), &numDevices, devices.get());
	if (ret != VK_SUCCESS) {
		vkm::ePrintf("Failed to get list of GPU devices: %s", vkm::vk::reflect::toString(ret).cStr());
		return ret;
	}
	VKM_UUID_INDEX_TYPE index;
	memcpy(&index, wantUUID + VKM_UUID_INDEX_OFFSET, sizeof(VKM_UUID_INDEX_TYPE));
	if (numDevices > uint32_t(index)) {
		auto uuid = vkm::vk::device::generateUUID(devices[index], index);
		if (memcmp(wantUUID, uuid.get(), VK_UUID_SIZE) == 0) {
			*physicalDevice = devices[index];
			return VK_SUCCESS;
		}
	}
	vkm::std::stringbuilder<char> error;
	error.write("Failed to find device with UUID: ");
	for (size_t i = 0; i < VK_UUID_SIZE; i++) {
		error.write("%.2X", wantUUID[i]);
	}
	vkm::ePrintf(error.cStr());
	*physicalDevice = VK_NULL_HANDLE;
	return VK_ERROR_DEVICE_LOST;
}
VKM_FN VkResult vkm_initDevice(vkm_deviceInitInfo info, vkm_device* instanceHandle) {
	auto* instance = new (::std::nothrow)::vkm::vk::device::instance(info);
	static constexpr vkm::std::array deviceSetups = {
		&vkm::vk::device::setupProperties,
		&vkm::vk::device::setupVKFNs,
		&vkm::vk::device::setupVMA,
	};
	for (auto setup : deviceSetups) {
		setup(instance);
	}
	*instanceHandle = instance->handle();
	return VK_SUCCESS;
}
VKM_FN void vkm_destroyDevice(vkm_device instanceHandle) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	delete instance;
}
VKM_FN void vkm_device_getVkDevice(vkm_device instanceHandle, VkPhysicalDevice* physicalDevice, VkDevice* device) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	if (physicalDevice != nullptr) {
		*physicalDevice = instance->vkPhysicalDevice;
	}
	if (device != nullptr) {
		*device = instance->vkDevice;
	}
}
VKM_FN void vkm_device_getProperties(vkm_device instanceHandle, vkm_device_properties* properties) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	*properties = instance->properties;
}
VKM_FN PFN_vkVoidFunction vkm_device_getProcAddr(vkm_device instanceHandle, vkm_device_vkfn_id fn) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	return instance->procAddr(fn);
}
VKM_FN void vkm_device_getDispatchTable(vkm_device instanceHandle, vkm_device_dispatchTable* table) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

#undef VKM_VKFN
#define VKM_VKFN(FN) table->FN = (PFN_##FN)vkm_getProcAddr(VKM_VKFN_##FN);
#include "vkm/inc/vkfn_dispatch_instance.inc"  // IWYU pragma: keep

#undef VKM_VKFN
#define VKM_VKFN(FN) table->FN = (PFN_##FN)instance->procAddr(VKM_DEVICE_VKFN_##FN);
#include "vkm/inc/vkfn_dispatch_device.inc"	 // IWYU pragma: keep

#undef VKM_VKFN
#define VKM_VKFN(FN) ((PFN_##FN)vkm_getProcAddr(VKM_VKFN_##FN))
}
VKM_FN VkResult vkm_device_waitIdle(vkm_device instanceHandle) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	return VK_PROC_DEVICE(instance, vkDeviceWaitIdle)(instance->vkDevice);
}
}
