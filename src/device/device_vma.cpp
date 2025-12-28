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

#include <stdint.h>

#include "vkm/std/utility.hpp"
#include "vkm/std/stdlib.hpp"

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "reflect_const.hpp"
#include "device/device.hpp"
#include "device/vma/vma.hpp"

namespace vkm::vk::device {
void setupVMA(vkm::vk::device::instance* device) noexcept {
	// VMA version check to make sure we update the function struct.
	// NOLINTNEXTLINE(misc-redundant-expression)
	static_assert(VMA_VULKAN_VERSION == 1004000);
	const VmaVulkanFunctions vkFns = {
		VK_PROC(vkGetInstanceProcAddr),
		VK_PROC(vkGetDeviceProcAddr),
		VK_PROC(vkGetPhysicalDeviceProperties),
		VK_PROC(vkGetPhysicalDeviceMemoryProperties),
		VK_PROC_DEVICE(device, vkAllocateMemory),
		VK_PROC_DEVICE(device, vkFreeMemory),
		VK_PROC_DEVICE(device, vkMapMemory),
		VK_PROC_DEVICE(device, vkUnmapMemory),
		VK_PROC_DEVICE(device, vkFlushMappedMemoryRanges),
		VK_PROC_DEVICE(device, vkInvalidateMappedMemoryRanges),
		VK_PROC_DEVICE(device, vkBindBufferMemory),
		VK_PROC_DEVICE(device, vkBindImageMemory),
		VK_PROC_DEVICE(device, vkGetBufferMemoryRequirements),
		VK_PROC_DEVICE(device, vkGetImageMemoryRequirements),
		VK_PROC_DEVICE(device, vkCreateBuffer),
		VK_PROC_DEVICE(device, vkDestroyBuffer),
		VK_PROC_DEVICE(device, vkCreateImage),
		VK_PROC_DEVICE(device, vkDestroyImage),
		VK_PROC_DEVICE(device, vkCmdCopyBuffer),
		VK_PROC_DEVICE(device, vkGetBufferMemoryRequirements2),
		VK_PROC_DEVICE(device, vkGetImageMemoryRequirements2),
		VK_PROC_DEVICE(device, vkBindBufferMemory2),
		VK_PROC_DEVICE(device, vkBindImageMemory2),
		VK_PROC(vkGetPhysicalDeviceMemoryProperties2),
		VK_PROC_DEVICE(device, vkGetDeviceBufferMemoryRequirements),
		VK_PROC_DEVICE(device, vkGetDeviceImageMemoryRequirements),
	};

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = device->vkPhysicalDevice;
	allocatorInfo.device = device->vkDevice;
	allocatorInfo.pVulkanFunctions = &vkFns;
	allocatorInfo.instance = vkm::vkInstance();
	allocatorInfo.vulkanApiVersion = device->properties.api;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT
						  | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	const VkResult ret = vmaCreateAllocator(&allocatorInfo, &device->vma.allocator);
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed to init VMA: %s", vkm::vk::reflect::toString(ret).cStr());
	}

	device->vma.noBARMemoryTypeBits = 0;
	device->vma.barMemoryTypeBits = 0;
	VkPhysicalDeviceMemoryProperties2 properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
	VK_PROC(vkGetPhysicalDeviceMemoryProperties2)(device->vkPhysicalDevice, &properties);
	for (uint32_t i = 0; i < properties.memoryProperties.memoryTypeCount; i++) {
		if (vkm::std::cmpBitFlagsContains(properties.memoryProperties.memoryTypes[i].propertyFlags,
										  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
			device->vma.barMemoryTypeBits |= 1 << i;
		} else {
			device->vma.noBARMemoryTypeBits |= 1 << i;
		}
	}
}

void destroyVMA(vkm::vk::device::instance* device) noexcept {
	vmaDestroyAllocator(device->vma.allocator);
}
}  // namespace vkm::vk::device
