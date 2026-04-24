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

#include "vkm/vkm.h"  // IWYU pragma: associated

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vkm/std/stdlib.hpp"
#include "vkm/std/string.hpp"

#include "vkm.hpp"
#include "vklog.hpp"
#include "reflect_const.hpp"
#include "device/device.hpp"
#include "device/vma/vma.hpp"

VKM_FN void vkm_createHostBuffer(vkm_device instanceHandle, vkm_string name, VkBufferCreateInfo info, vkm_hostBuffer* b) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		allocCreateInfo.memoryTypeBits = instance->vma.noBARMemoryTypeBits;

		VkResult ret = vmaCreateBuffer(instance->vma.allocator, &info, &allocCreateInfo, &b->vkBuffer,
									   reinterpret_cast<VmaAllocation*>(&b->allocation), nullptr);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create buffer: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}

		ret = vmaMapMemory(instance->vma.allocator, reinterpret_cast<VmaAllocation>(b->allocation), &b->ptr);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to map buffer: %s", vkm::vk::reflect::toString(ret).cStr());
		}
	}
	vkm::std::debugRun([=]() {
		vkm::std::stringbuilder builder;
		builder.write(name).write("_hostBuffer");
		vkm::vk::debugLabel(instance->vkDevice, b->vkBuffer, builder.cStr());

		builder.write("_allocation");
		vmaSetAllocationName(instance->vma.allocator, reinterpret_cast<VmaAllocation>(b->allocation), builder.cStr());
	});
}
VKM_FN void vkm_destroyHostBuffer(vkm_device instanceHandle, vkm_hostBuffer b) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	vmaUnmapMemory(instance->vma.allocator, reinterpret_cast<VmaAllocation>(b.allocation));
	vmaDestroyBuffer(instance->vma.allocator, b.vkBuffer, reinterpret_cast<VmaAllocation>(b.allocation));
}
VKM_FN void vkm_hostBuffer_write(vkm_device, vkm_hostBuffer buffer, size_t offset, size_t sz, void* data) {
	memcpy(static_cast<void*>(static_cast<uint8_t*>(buffer.ptr) + offset), data, sz);
}
VKM_FN void vkm_hostBuffer_read(vkm_device, vkm_hostBuffer buffer, size_t offset, size_t sz, void* data) {
	memcpy(data, static_cast<void*>(static_cast<uint8_t*>(buffer.ptr) + offset), sz);
}
VKM_FN void vkm_createDeviceBuffer(vkm_device instanceHandle, vkm_string name, VkBufferCreateInfo info, vkm_deviceBuffer* b) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		allocCreateInfo.memoryTypeBits = instance->vma.noBARMemoryTypeBits;

		const VkResult ret = vmaCreateBuffer(instance->vma.allocator, &info, &allocCreateInfo, &b->vkBuffer,
											 reinterpret_cast<VmaAllocation*>(&b->allocation), nullptr);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create buffer: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
	}
	vkm::std::debugRun([=]() {
		vkm::std::stringbuilder builder;
		builder.write(name).write("_deviceBuffer");
		vkm::vk::debugLabel(instance->vkDevice, b->vkBuffer, builder.cStr());

		builder.write("_allocation");
		vmaSetAllocationName(instance->vma.allocator, reinterpret_cast<VmaAllocation>(b->allocation), builder.cStr());
	});
}
VKM_FN void vkm_destroyDeviceBuffer(vkm_device instanceHandle, vkm_deviceBuffer b) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	vmaDestroyBuffer(instance->vma.allocator, b.vkBuffer, reinterpret_cast<VmaAllocation>(b.allocation));
}
