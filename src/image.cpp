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

#include "vkm/std/stdlib.hpp"
#include "vkm/std/string.hpp"
#include "vkm/std/utility.hpp"

#include "vkm.hpp"
#include "vklog.hpp"
#include "reflect_const.hpp"
#include "device/device.hpp"
#include "device/vma/vma.hpp"

extern "C" {
VKM_FN void vkm_getFormatProperties3(vkm_device instanceHandle, VkFormat format, VkFormatProperties3* properties) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	properties->sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
	VkFormatProperties2 formatProperties2 = {
		.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
		.pNext = properties,
	};
	VK_PROC(vkGetPhysicalDeviceFormatProperties2)
	(instance->vkPhysicalDevice, format, &formatProperties2);
}
VKM_FN VkResult vkm_getImageFormatProperties2(vkm_device instanceHandle, VkPhysicalDeviceImageFormatInfo2 info,
											  VkImageFormatProperties2* properties) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
	properties->sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
	return VK_PROC(vkGetPhysicalDeviceImageFormatProperties2)(instance->vkPhysicalDevice, &info, properties);
}
VKM_FN VkBool32 vkm_getFormatHasImageUsageFlags(vkm_device instanceHandle, VkFormat format, VkImageUsageFlags usage) {
	VkFormatProperties3 properties = {.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3};
	vkm_getFormatProperties3(instanceHandle, format, &properties);
	if (vkm::std::cmpBitFlagsContains(
			properties.optimalTilingFeatures, vkm::vk::reflect::mapVkImageUsageFlagsToVkFormatFeatureFlags2(usage))) {
		return VK_TRUE;
	}
	return VK_FALSE;
}
VKM_FN void vkm_createImage(vkm_device instanceHandle, vkm_string name, VkImageCreateInfo info, vkm_image* t) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		allocCreateInfo.memoryTypeBits = instance->vma.noBARMemoryTypeBits;

		const VkResult ret = vmaCreateImage(instance->vma.allocator, &info, &allocCreateInfo, &t->vkImage,
											reinterpret_cast<VmaAllocation*>(&t->allocation), nullptr);
		if (ret != VK_SUCCESS) {
			vkm::fatal(
				vkm::std::sourceLocation::current(), "Failed to create image: %s", vkm::vk::reflect::toString(ret).cStr());
		}
	}
	vkm::std::debugRun([=]() {
		vkm::std::stringbuilder builder;
		builder.write(name).write("_image");
		vkm::vk::debugLabel(instance->vkDevice, t->vkImage, builder.cStr());

		builder.write("_allocation");
		vmaSetAllocationName(instance->vma.allocator, reinterpret_cast<VmaAllocation>(t->allocation), builder.cStr());
	});
}
VKM_FN void vkm_destroyImage(vkm_device instanceHandle, vkm_image t) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	vmaDestroyImage(instance->vma.allocator, t.vkImage, reinterpret_cast<VmaAllocation>(t.allocation));
}
VKM_FN void vkm_createImageView(vkm_device instanceHandle, vkm_string name, VkImageViewCreateInfo info, VkImageView* view) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	{
		const VkResult ret = VK_PROC_DEVICE(instance, vkCreateImageView)(instance->vkDevice, &info, nullptr, view);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create image view: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
	}
	vkm::std::debugRun([=]() {
		vkm::std::stringbuilder builder;
		builder.write(name).write("_imageView");
		vkm::vk::debugLabel(instance->vkDevice, *view, builder.cStr());
	});
}
VKM_FN void vkm_destroyImageView(vkm_device instanceHandle, VkImageView view) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	VK_PROC_DEVICE(instance, vkDestroyImageView)(instance->vkDevice, view, nullptr);
}
VKM_FN void vkm_createSampler(vkm_device instanceHandle, vkm_string name, VkSamplerCreateInfo info, VkSampler* sampler) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	{
		const VkResult ret = VK_PROC_DEVICE(instance, vkCreateSampler)(instance->vkDevice, &info, nullptr, sampler);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create sampler: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
	}
	vkm::std::debugRun([=]() {
		vkm::std::stringbuilder builder;
		builder.write(name).write("_sampler");
		vkm::vk::debugLabel(instance->vkDevice, *sampler, builder.cStr());
	});
}
VKM_FN void vkm_destroySampler(vkm_device instanceHandle, VkSampler sampler) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	VK_PROC_DEVICE(instance, vkDestroySampler)(instance->vkDevice, sampler, nullptr);
}
}
