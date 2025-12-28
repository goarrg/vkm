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

#include "swapchain/swapchain.hpp"

#include <stdint.h>
#include <stddef.h>
#include <new>

#include "vkm/std/stdlib.hpp"
#include "vkm/std/algorithm.hpp"
#include "vkm/std/time.hpp"
#include "vkm/std/defer.hpp"
#include "vkm/std/utility.hpp"
#include "vkm/std/array.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/string.hpp"

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "vklog.hpp"
#include "reflect_struct.hpp"
#include "reflect_const.hpp"
#include "device/device.hpp"

#define HANDLE_SURFACE_ERROR(ret, fmt, ...)                                    \
	switch (ret) {                                                             \
		case VK_SUCCESS:                                                       \
			break;                                                             \
                                                                               \
		case VK_ERROR_SURFACE_LOST_KHR:                                        \
			return ret;                                                        \
                                                                               \
		default:                                                               \
			vkm::fatal(vkm::std::sourceLocation::current(), fmt, __VA_ARGS__); \
			break;                                                             \
	}

namespace vkm::vk {
VkResult swapchain::requirements::findPresentMode(swapchain* swapchain) noexcept {
	swapchain->vkPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t numPresentModes = 0;
	VkResult ret = VKM_VKFN(vkGetPhysicalDeviceSurfacePresentModesKHR)(
		swapchain->instance->vkPhysicalDevice, swapchain->vkSurface, &numPresentModes, nullptr);
	HANDLE_SURFACE_ERROR(ret, "Failed to get surface present modes: %s", vkm::vk::reflect::toString(ret).cStr());

	vkm::std::vector<VkPresentModeKHR> presentModes(numPresentModes);
	ret = VKM_VKFN(vkGetPhysicalDeviceSurfacePresentModesKHR)(
		swapchain->instance->vkPhysicalDevice, swapchain->vkSurface, &numPresentModes, presentModes.get());
	HANDLE_SURFACE_ERROR(ret, "Failed to get surface present modes: %s", vkm::vk::reflect::toString(ret).cStr());

	{
		vkm::std::stringbuilder builder;
		for (size_t i = 0; auto& presentMode : presentModes) {
			builder << "\n[" << i++ << "] " << vkm::vk::reflect::toString(presentMode);
		}
		vkm::iPrintf("Found surface present modes: %s", builder.cStr());
	}
	swapchain->vkPresentMode = [&]() -> auto {
		for (auto want : swapchain->requirements.preferredPresentModes) {
			for (size_t i = 0; i < presentModes.size(); i++) {
				auto& have = presentModes[i];
				if (have == want) {
					vkm::iPrintf("Selected present mode: [%d]", i);
					return have;
				}
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}();
	return VK_SUCCESS;
}
VkResult swapchain::requirements::findCapabilities(swapchain* swapchain) noexcept {
	if (swapchain->instance->optionalFeatures.hasEXTSwapchainMaint1) {
		const VkSurfacePresentModeEXT surfacePresentModeInfo = {
			.sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
			.presentMode = swapchain->vkPresentMode,
		};
		const VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
			.pNext = &surfacePresentModeInfo,
			.surface = swapchain->vkSurface,
		};

		VkSurfacePresentModeCompatibilityEXT surfacePresentModeCompatibilityInfo = {
			.sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT,
		};
		VkSurfaceCapabilities2KHR surfaceCapabilities2 = {
			.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
			.pNext = &surfacePresentModeCompatibilityInfo,
		};

		VkResult ret = VKM_VKFN(vkGetPhysicalDeviceSurfaceCapabilities2KHR)(
			swapchain->instance->vkPhysicalDevice, &surfaceInfo2, &surfaceCapabilities2);
		HANDLE_SURFACE_ERROR(ret, "Failed to get surface capabilities: %s", vkm::vk::reflect::toString(ret).cStr());

		swapchain->compatiblePresentModes.resize(surfacePresentModeCompatibilityInfo.presentModeCount);
		surfacePresentModeCompatibilityInfo.pPresentModes = swapchain->compatiblePresentModes.get();

		ret = VKM_VKFN(vkGetPhysicalDeviceSurfaceCapabilities2KHR)(
			swapchain->instance->vkPhysicalDevice, &surfaceInfo2, &surfaceCapabilities2);
		HANDLE_SURFACE_ERROR(ret, "Failed to get surface capabilities: %s", vkm::vk::reflect::toString(ret).cStr());

		swapchain->vkSurfaceCapabilities = surfaceCapabilities2.surfaceCapabilities;
	} else {
		const VkResult ret = VKM_VKFN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(
			swapchain->instance->vkPhysicalDevice, swapchain->vkSurface, &swapchain->vkSurfaceCapabilities);
		HANDLE_SURFACE_ERROR(ret, "Failed to get surface capabilities: %s", vkm::vk::reflect::toString(ret).cStr());
	}
	return VK_SUCCESS;
}
VkResult swapchain::requirements::findSurfaceFormat(swapchain* swapchain) noexcept {
	uint32_t numSurfaceFormats = 0;
	VkResult ret = VKM_VKFN(vkGetPhysicalDeviceSurfaceFormatsKHR)(
		swapchain->instance->vkPhysicalDevice, swapchain->vkSurface, &numSurfaceFormats, nullptr);
	HANDLE_SURFACE_ERROR(ret, "Failed to get surface formats: %s", vkm::vk::reflect::toString(ret).cStr());

	vkm::std::vector<VkSurfaceFormatKHR> surfaceFormats(numSurfaceFormats);
	ret = VKM_VKFN(vkGetPhysicalDeviceSurfaceFormatsKHR)(
		swapchain->instance->vkPhysicalDevice, swapchain->vkSurface, &numSurfaceFormats, surfaceFormats.get());
	HANDLE_SURFACE_ERROR(ret, "Failed to get surface formats: %s", vkm::vk::reflect::toString(ret).cStr());

	{
		vkm::std::stringbuilder builder;
		for (size_t i = 0; auto& surfaceFormat : surfaceFormats) {
			builder << "\n[" << i++ << "] " << vkm::vk::reflect::toString(surfaceFormat.format) << ", "
					<< vkm::vk::reflect::toString(surfaceFormat.colorSpace);
		}
		vkm::iPrintf("Found surface formats: %s", builder.cStr());
	}
	swapchain->vkSurfaceFormat = [&]() -> auto {
		for (auto want : swapchain->requirements.preferredSurfaceFormats) {
			for (size_t i = 0; i < surfaceFormats.size(); i++) {
				auto& have = surfaceFormats[i];
				if (have.format == want.format && have.colorSpace == want.colorSpace) {
					if (vkm_getFormatHasImageUsageFlags(
							swapchain->instance->handle(), have.format, swapchain->requirements.requiredUsage)
						!= VK_TRUE) {
						continue;
					}
					if (!vkm::std::cmpBitFlagsContains(
							swapchain->vkSurfaceCapabilities.supportedUsageFlags, swapchain->requirements.requiredUsage)) {
						continue;
					}
					vkm::iPrintf("Selected format: [%d]", i);
					return have;
				}
			}
		}
		vkm::ePrintf("No known surface formats with required usage [0x%X] found", swapchain->requirements.requiredUsage);
		return VkSurfaceFormatKHR{};
	}();
	if (swapchain->vkSurfaceFormat.format == VK_FORMAT_UNDEFINED) {
		return VK_ERROR_FORMAT_NOT_SUPPORTED;
	}

	return VK_SUCCESS;
}
swapchain::image::~image() noexcept {
	if (this->fence != VK_NULL_HANDLE) {
		const VkResult ret = VK_PROC_DEVICE(this->instance, vkWaitForFences)(
			this->instance->vkDevice, 1, &this->fence, VK_TRUE, vkm::std::time::second);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to wait on swapchain: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
		this->instance->syncObjectManager.releaseFence(this->fence);
		this->instance->syncObjectManager.releaseBinarySemaphore(this->surfaceReleaseSemaphore);
	} else {
		VK_PROC_DEVICE(this->instance, vkDestroySemaphore)(this->instance->vkDevice, this->surfaceReleaseSemaphore, nullptr);
	}
	VK_PROC_DEVICE(this->instance, vkDestroyImageView)(this->instance->vkDevice, this->vkImageView, nullptr);
}
VkResult swapchain::acquire(VkSemaphore semaphore, vkm_swapcain_image* outImage) noexcept {
	if (this->imageIndex != UINT32_MAX) {
		vkm::fatal("Cannot acquire swapchain before preseenting the previous acquire");
	}
	const VkResult ret = VKM_DEVICE_VKFN(this->instance, vkAcquireNextImageKHR)(
		this->instance->vkDevice, this->vkSwapchain, vkm::std::time::second, semaphore, VK_NULL_HANDLE, &this->imageIndex);
	switch (ret) {
		case VK_SUCCESS:
		case VK_SUBOPTIMAL_KHR: {
			auto& image = this->images[this->imageIndex];
			*outImage = vkm_swapcain_image{
				.index = this->imageIndex,
				.vkImage = image.vkImage,
				.vkImageView = image.vkImageView,
			};
			return ret;
		}

		case VK_ERROR_OUT_OF_DATE_KHR:
		case VK_ERROR_SURFACE_LOST_KHR:
			*outImage = vkm_swapcain_image{};
			return ret;

		default:
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to acquire image: %s",
					   vkm::vk::reflect::toString(ret).cStr());
	}
	return VK_ERROR_UNKNOWN;
}
[[nodiscard]] VkSemaphore swapchain::semaphore() noexcept {
	if (this->imageIndex == UINT32_MAX) {
		vkm::fatal("Cannot present swapchain before acquiring");
	}

	auto& image = this->images[this->imageIndex];
	return image.surfaceReleaseSemaphore;
}
[[nodiscard]] VkResult swapchain::present(VkQueue vkQueue) noexcept {
	if (this->imageIndex == UINT32_MAX) {
		vkm::fatal("Cannot present swapchain before acquiring");
	}

	auto& image = this->images[this->imageIndex];
	VkSwapchainPresentFenceInfoEXT presentFenceInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
		.swapchainCount = 1,
		.pFences = &image.fence,
	};
	if (image.fence != VK_NULL_HANDLE) {
		{
			const VkResult ret = VK_PROC_DEVICE(this->instance, vkWaitForFences)(
				this->instance->vkDevice, 1, &image.fence, VK_TRUE, vkm::std::time::second);
			if (ret != VK_SUCCESS) {
				vkm::fatal(vkm::std::sourceLocation::current(), "Failed to wait for fence: %s",
						   vkm::vk::reflect::toString(ret).cStr());
			}
		}
		{
			const VkResult ret = VK_PROC_DEVICE(this->instance, vkResetFences)(this->instance->vkDevice, 1, &image.fence);
			if (ret != VK_SUCCESS) {
				vkm::fatal(vkm::std::sourceLocation::current(), "Failed to reset fence: %s",
						   vkm::vk::reflect::toString(ret).cStr());
			}
		}
		vkm::vk::reflect::appendVkStructureChain(
			this->pendingPresentChain, false, reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(&presentFenceInfo));
	}
	const VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = this->pendingPresentChain.size() > 0 ? this->pendingPresentChain.first().get() : nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &image.surfaceReleaseSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &this->vkSwapchain,
		.pImageIndices = &this->imageIndex,
	};
	{
		const VkResult ret = VKM_DEVICE_VKFN(this->instance, vkQueuePresentKHR)(vkQueue, &presentInfo);
		this->pendingPresentChain.resize(0);
		this->imageIndex = UINT32_MAX;
		switch (ret) {
			case VK_SUCCESS:
			case VK_SUBOPTIMAL_KHR:
			case VK_ERROR_OUT_OF_DATE_KHR:
			case VK_ERROR_SURFACE_LOST_KHR:
				return ret;

			default:
				vkm::fatal(vkm::std::sourceLocation::current(), "Failed to present frame: %s",
						   vkm::vk::reflect::toString(ret).cStr());
				break;
		}
	}
	return VK_ERROR_UNKNOWN;
}
}  // namespace vkm::vk

VKM_FN VkResult vkm_createSwapchain(vkm_device instanceHandle, vkm_string name, vkm_swapchainCreateInfo info, vkm_swapchain* swapchainHandle) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	auto* swapchain = new (::std::nothrow)::vkm::vk::swapchain();
	swapchain->instance = instance;
	swapchain->vkSurface = info.targetSurface;
	swapchain->vkSwapchain = VK_NULL_HANDLE;
	swapchain->vkPresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
	swapchain->imageIndex = UINT32_MAX;

	bool ok = true;
	DEFER([&]() {
		if (!ok) {
			delete swapchain;
			*swapchainHandle = nullptr;
		}
	});

	if (name.len != 0 && name.ptr != nullptr) {
		vkm::std::stringbuilder builder;
		builder << name << "_swapchain";
		swapchain->name = builder.str();
	} else {
		vkm::std::stringbuilder builder;
		builder.write("swapchain_%X_", swapchain->vkSurface);
		swapchain->name = builder.str();
	}

	swapchain->requirements.requiredUsage = info.requiredUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain->requirements.preferredImageCount = info.preferredImageCount;
	{
		swapchain->requirements.preferredSurfaceFormats.pushBack(info.numPreferredVkSurfaceFormats, info.pPreferredVkSurfaceFormats);
		if (swapchain->requirements.preferredSurfaceFormats.size() == 0) {
			swapchain->requirements.preferredSurfaceFormats = vkm::std::array{
				VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
				VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
		}
	}

	{
		const VkResult ret = vkm_swapchain_changeVkPresentMode(
			swapchain->handle(), info.numPreferredVkPresentModes, info.pPreferredVkPresentModes, info.extent);
		if (ret != VK_SUCCESS) {
			ok = false;
			return ret;
		}
	}

	*swapchainHandle = swapchain->handle();
	return VK_SUCCESS;
}
VKM_FN void vkm_destroySwapchain(vkm_swapchain swapchainHandle) {
	auto* swapchain = vkm::vk::swapchain::fromHandle(swapchainHandle);
	if (!swapchain->instance->optionalFeatures.hasEXTSwapchainMaint1) {
		vkm_device_waitIdle(swapchain->instance->handle());
	}
	swapchain->images.resize(0);
	VKM_DEVICE_VKFN(swapchain->instance, vkDestroySwapchainKHR)(swapchain->instance->vkDevice, swapchain->vkSwapchain, nullptr);
	delete swapchain;
}
VKM_FN void vkm_swapchain_getProperties(vkm_swapchain swapchainHandle, vkm_swapchain_properties* prop) {
	auto* swapchain = vkm::vk::swapchain::fromHandle(swapchainHandle);
	*prop = vkm_swapchain_properties{
		.vkSurfaceFormat = swapchain->vkSurfaceFormat,
		.extent = swapchain->extent,
		.numImages = static_cast<uint32_t>(swapchain->images.size()),
	};
}
VKM_FN VkResult vkm_swapchain_resize(vkm_swapchain swapchainHandle, VkExtent2D extent) {
	auto* swapchain = vkm::vk::swapchain::fromHandle(swapchainHandle);

	if (!swapchain->instance->optionalFeatures.hasEXTSwapchainMaint1) {
		vkm_device_waitIdle(swapchain->instance->handle());
	}
	swapchain->images.resize(0);

	{
		const VkResult ret = vkm::vk::swapchain::requirements::findCapabilities(swapchain);
		if (ret != VK_SUCCESS) {
			return ret;
		}
	}
	{
		const VkResult ret = vkm::vk::swapchain::requirements::findSurfaceFormat(swapchain);
		if (ret != VK_SUCCESS) {
			return ret;
		}
	}
	{
		if (!vkm::std::cmpBitFlagsContains(swapchain->vkSurfaceCapabilities.supportedCompositeAlpha, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
			vkm::fatal("Failed to create swapchain: VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR is unsupported");
		}

		swapchain->extent = extent;
	}

	{
		const VkSwapchainPresentModesCreateInfoEXT presentModesCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT,
			.presentModeCount = static_cast<uint32_t>(swapchain->compatiblePresentModes.size()),
			.pPresentModes = swapchain->compatiblePresentModes.get(),
		};
		VkSwapchainCreateInfoKHR createInfo = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = swapchain->instance->optionalFeatures.hasEXTSwapchainMaint1 ? &presentModesCreateInfo : nullptr,
			.surface = swapchain->vkSurface,
			.minImageCount = vkm::std::max(swapchain->vkSurfaceCapabilities.minImageCount + 1, swapchain->requirements.preferredImageCount),
			.imageFormat = swapchain->vkSurfaceFormat.format,
			.imageColorSpace = swapchain->vkSurfaceFormat.colorSpace,
			.imageExtent = swapchain->extent,
			.imageArrayLayers = 1,
			.imageUsage = swapchain->requirements.requiredUsage,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.preTransform = swapchain->vkSurfaceCapabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = swapchain->vkPresentMode,
			.oldSwapchain = swapchain->vkSwapchain,
		};
		if ((swapchain->vkSurfaceCapabilities.maxImageCount != 0u)
			&& (createInfo.minImageCount > swapchain->vkSurfaceCapabilities.maxImageCount)) {
			createInfo.minImageCount = swapchain->vkSurfaceCapabilities.maxImageCount;
		}

		const VkResult ret = VKM_DEVICE_VKFN(swapchain->instance, vkCreateSwapchainKHR)(
			swapchain->instance->vkDevice, &createInfo, nullptr, &swapchain->vkSwapchain);
		HANDLE_SURFACE_ERROR(ret, "Failed to create swapchain: %s", vkm::vk::reflect::toString(ret).cStr());
		VKM_DEVICE_VKFN(swapchain->instance, vkDestroySwapchainKHR)(swapchain->instance->vkDevice, createInfo.oldSwapchain, nullptr);
		vkm::vk::debugLabel(swapchain->instance->vkDevice, swapchain->vkSwapchain, swapchain->name.cStr());
	}

	{
		uint32_t numImages = 0;
		VkResult ret = VKM_DEVICE_VKFN(swapchain->instance, vkGetSwapchainImagesKHR)(
			swapchain->instance->vkDevice, swapchain->vkSwapchain, &numImages, nullptr);
		HANDLE_SURFACE_ERROR(ret, "Failed to get swapchain images: %s", vkm::vk::reflect::toString(ret).cStr());

		vkm::std::vector<VkImage> swapChainImages(numImages);
		swapchain->images.resize(numImages);
		ret = VKM_DEVICE_VKFN(swapchain->instance, vkGetSwapchainImagesKHR)(
			swapchain->instance->vkDevice, swapchain->vkSwapchain, &numImages, swapChainImages.get());
		HANDLE_SURFACE_ERROR(ret, "Failed to get swapchain images: %s", vkm::vk::reflect::toString(ret).cStr());

		swapchain->images.resize(numImages);
		for (uint32_t i = 0; i < numImages; i++) {
			swapchain->images[i].instance = swapchain->instance;

			{
				const VkImageViewCreateInfo createInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = swapchain->images[i].vkImage = swapChainImages[i],
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format =  swapchain->vkSurfaceFormat.format,
					.components =
						VkComponentMapping{
							.r = VK_COMPONENT_SWIZZLE_IDENTITY,
							.g = VK_COMPONENT_SWIZZLE_IDENTITY,
							.b = VK_COMPONENT_SWIZZLE_IDENTITY,
							.a = VK_COMPONENT_SWIZZLE_IDENTITY,
						},
					.subresourceRange =
						VkImageSubresourceRange{
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1,
						},
				};

				const VkResult ret = VK_PROC_DEVICE(swapchain->instance, vkCreateImageView)(
					swapchain->instance->vkDevice, &createInfo, nullptr, &swapchain->images[i].vkImageView);
				HANDLE_SURFACE_ERROR(ret, "Failed to create swapchain image view: %s", vkm::vk::reflect::toString(ret).cStr());
				vkm::std::debugRun([=]() {
					vkm::std::stringbuilder builder;
					builder << swapchain->name << "_image_" << i;

					vkm::vk::debugLabel(swapchain->instance->vkDevice, swapchain->images[i].vkImage, builder.cStr());
				});
				vkm::std::debugRun([=]() {
					vkm::std::stringbuilder builder;
					builder << swapchain->name << "_imageView_" << i;
					vkm::vk::debugLabel(swapchain->instance->vkDevice, swapchain->images[i].vkImageView, builder.cStr());
				});
			}
			{
				swapchain->images[i].surfaceReleaseSemaphore = swapchain->instance->syncObjectManager.acquireBinarySemaphore();
				vkm::std::debugRun([=]() {
					vkm::std::stringbuilder builder;
					builder << swapchain->name << "_semaphoreBinary_surfaceRelease_image_" << i;
					vkm::vk::debugLabel(
						swapchain->instance->vkDevice, swapchain->images[i].surfaceReleaseSemaphore, builder.cStr());
				});
			}
			if (swapchain->instance->optionalFeatures.hasEXTSwapchainMaint1) {
				swapchain->images[i].fence = swapchain->instance->syncObjectManager.acquireFence(true);
				vkm::std::debugRun([=]() {
					vkm::std::stringbuilder builder;
					builder << swapchain->name << "_fence_image_" << i;
					vkm::vk::debugLabel(swapchain->instance->vkDevice, swapchain->images[i].fence, builder.cStr());
				});
			}
		}
	}

	return VK_SUCCESS;
}
VKM_FN VkResult vkm_swapchain_changeVkPresentMode(vkm_swapchain swapchainHandle, size_t numPresentModes,
												  VkPresentModeKHR* presentModes, VkExtent2D extent) {
	auto* swapchain = vkm::vk::swapchain::fromHandle(swapchainHandle);
	const VkPresentModeKHR oldMode = swapchain->vkPresentMode;
	{
		swapchain->requirements.preferredPresentModes.resize(0);
		swapchain->requirements.preferredPresentModes.pushBack(numPresentModes, presentModes);
		if (swapchain->requirements.preferredPresentModes.size() == 0) {
			swapchain->requirements.preferredPresentModes = vkm::std::array{VK_PRESENT_MODE_FIFO_RELAXED_KHR};
		}
	}
	{
		const VkResult ret = vkm::vk::swapchain::requirements::findPresentMode(swapchain);
		if (ret != VK_SUCCESS) {
			return ret;
		}
	}
	if ((swapchain->extent.width != extent.width) || (swapchain->extent.height != extent.height)) {
		return vkm_swapchain_resize(swapchainHandle, extent);
	}
	if (oldMode == swapchain->vkPresentMode) {
		return VK_SUCCESS;
	}
	if (swapchain->instance->optionalFeatures.hasEXTSwapchainMaint1) {
		const size_t i = vkm::std::linearSearch(swapchain->compatiblePresentModes.size(), [&](size_t i) -> bool {
			return swapchain->compatiblePresentModes[i] == swapchain->vkPresentMode;
		});
		if (i < swapchain->compatiblePresentModes.size()) {
			VkSwapchainPresentModeInfoKHR info = {
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT,
				.swapchainCount = 1,
				.pPresentModes = &swapchain->vkPresentMode,
			};
			swapchain->pendingPresentChain = vkm::std::move(
				vkm::vk::reflect::cloneVkStructureChain(reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(&info)));
			return VK_SUCCESS;
		}
	}
	return vkm_swapchain_resize(swapchainHandle, extent);
}
