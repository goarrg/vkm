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

#include "vkm/std/stdlib.hpp"  // IWYU pragma: keep
#include "vkm/std/vector.hpp"  // IWYU pragma: keep
#include "vkm/std/string.hpp"  // IWYU pragma: keep

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "reflect_const.hpp"  // IWYU pragma: keep

namespace vkm::vk {
template <typename... Args>
inline static void debugLabelBegin([[maybe_unused]] VkQueue q, [[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) noexcept {
#ifndef NDEBUG
	VkDebugUtilsLabelEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;

	vkm::std::vector<char> buf;	 // NOLINT(misc-const-correctness)

	if constexpr (sizeof...(Args) > 0) {
		const int n = snprintf(nullptr, 0, fmt, args...) + 1;
		buf.resize(n);
		snprintf(buf.get(), n, fmt, args...);
		nameInfo.pLabelName = buf.get();
	} else {
		nameInfo.pLabelName = fmt;
	}

	VK_DEBUG_PROC(vkQueueBeginDebugUtilsLabelEXT)(q, &nameInfo);
#endif
}
inline static void debugLabelEnd([[maybe_unused]] VkQueue q) noexcept {
#ifndef NDEBUG
	VK_DEBUG_PROC(vkQueueEndDebugUtilsLabelEXT)(q);
#endif
}

template <typename... Args>
inline static void debugLabelBegin([[maybe_unused]] VkCommandBuffer cb, [[maybe_unused]] const char* fmt,
								   [[maybe_unused]] Args... args) noexcept {
#ifndef NDEBUG
	VkDebugUtilsLabelEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;

	vkm::std::vector<char> buf;	 // NOLINT(misc-const-correctness)

	if constexpr (sizeof...(Args) > 0) {
		int n = snprintf(nullptr, 0, fmt, args...) + 1;
		buf.resize(n);
		snprintf(buf.get(), n, fmt, args...);
		nameInfo.pLabelName = buf.get();
	} else {
		nameInfo.pLabelName = fmt;
	}

	VK_DEBUG_PROC(vkCmdBeginDebugUtilsLabelEXT)(cb, &nameInfo);
#endif
}
inline static void debugLabelEnd([[maybe_unused]] VkCommandBuffer cb) noexcept {
#ifndef NDEBUG
	VK_DEBUG_PROC(vkCmdEndDebugUtilsLabelEXT)(cb);
#endif
}

inline static void debugLabel([[maybe_unused]] VkDevice vkDevice, [[maybe_unused]] VkObjectType type,
							  [[maybe_unused]] uint64_t handle, [[maybe_unused]] const char* fmt) noexcept {
#ifndef NDEBUG
	VkDebugUtilsObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = type;
	nameInfo.objectHandle = handle;
	nameInfo.pObjectName = fmt;

	const VkResult ret = VK_DEBUG_PROC(vkSetDebugUtilsObjectNameEXT)(vkDevice, &nameInfo);
	if (ret != VK_SUCCESS) {
		vkm::std::abort((vkm::std::string("vkSetDebugUtilsObjectNameEXT: ") + vkm::vk::reflect::toString(ret)).cStr());
	}
#endif
}

template <typename... Args>
inline static void debugLabel([[maybe_unused]] VkDevice vkDevice, [[maybe_unused]] VkObjectType type, [[maybe_unused]] uint64_t handle,
							  [[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) noexcept {
#ifndef NDEBUG
	VkDebugUtilsObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = type;
	nameInfo.objectHandle = handle;

	vkm::std::vector<char> buf;	 // NOLINT(misc-const-correctness)

	if constexpr (sizeof...(Args) > 0) {
		const int n = snprintf(nullptr, 0, fmt, args...) + 1;
		buf.resize(n);
		snprintf(buf.get(), n, fmt, args...);
		nameInfo.pObjectName = buf.get();
	} else {
		nameInfo.pObjectName = fmt;
	}

	const VkResult ret = VK_DEBUG_PROC(vkSetDebugUtilsObjectNameEXT)(vkDevice, &nameInfo);
	if (ret != VK_SUCCESS) {
		vkm::std::abort((vkm::std::string("vkSetDebugUtilsObjectNameEXT: ") + vkm::vk::reflect::toString(ret)).cStr());
	}
#endif
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkQueue queue, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64_t>(queue), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkFence fence, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_FENCE, reinterpret_cast<uint64_t>(fence), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkCommandPool pool, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64_t>(pool), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkBuffer buffer, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(buffer), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkImage image, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkImageView view, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(view), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkSampler sampler, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<uint64_t>(sampler), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkSemaphore semaphore, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(semaphore), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkDescriptorSetLayout layout, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<uint64_t>(layout), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkDescriptorPool pool, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_DESCRIPTOR_POOL, reinterpret_cast<uint64_t>(pool), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkDescriptorSet set, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<uint64_t>(set), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkPipelineLayout layout, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(layout), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkPipeline pipeline, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(pipeline), fmt, args...);
}

template <typename... Args>
inline static void debugLabel(VkDevice vkDevice, VkSwapchainKHR swapchain, const char* fmt, Args... args) noexcept {
	debugLabel(vkDevice, VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<uint64_t>(swapchain), fmt, args...);
}
}  // namespace vkm::vk
