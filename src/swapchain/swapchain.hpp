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

#include "vkm/std/string.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/memory.hpp"

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "reflect_struct.hpp"
#include "device/device.hpp"

namespace vkm::vk {
struct swapchain {
	vkm::vk::device::instance* instance;
	vkm::std::string<char> name;

	struct requirements {
		VkImageUsageFlags requiredUsage;
		uint32_t preferredImageCount;
		vkm::std::vector<VkSurfaceFormatKHR> preferredSurfaceFormats;
		vkm::std::vector<VkPresentModeKHR> preferredPresentModes;

		static VkResult findPresentMode(swapchain*) noexcept;
		static VkResult findCapabilities(swapchain* swapchain) noexcept;
		static VkResult findSurfaceFormat(swapchain*) noexcept;
	} requirements;

	VkSurfaceKHR vkSurface;
	VkSurfaceFormatKHR vkSurfaceFormat;

	VkPresentModeKHR vkPresentMode;
	vkm::std::vector<VkPresentModeKHR> compatiblePresentModes;
	VkSurfaceCapabilitiesKHR vkSurfaceCapabilities;

	VkExtent2D extent = {};
	VkSwapchainKHR vkSwapchain;

	vkm::std::vector<vkm::std::smartPtr<vkm::vk::reflect::vkStructureChain>> pendingPresentChain;

	struct image {
		vkm::vk::device::instance* instance = nullptr;

		VkImage vkImage = VK_NULL_HANDLE;
		VkImageView vkImageView = VK_NULL_HANDLE;
		VkSemaphore surfaceReleaseSemaphore = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;

		image() noexcept = default;
		~image() noexcept;
	};
	vkm::std::vector<image> images;
	uint32_t imageIndex;

	[[nodiscard]] VkResult acquire(VkSemaphore, vkm_swapcain_image*) noexcept;
	[[nodiscard]] VkSemaphore semaphore() noexcept;
	[[nodiscard]] VkResult present(VkQueue) noexcept;

	[[nodiscard]] vkm_swapchain handle() noexcept { return reinterpret_cast<vkm_swapchain>(this); }
	[[nodiscard]] static swapchain* fromHandle(vkm_swapchain handle) noexcept {
		return reinterpret_cast<swapchain*>(handle);
	}
};
}  // namespace vkm::vk
