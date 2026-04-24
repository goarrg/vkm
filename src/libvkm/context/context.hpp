/*
Copyright 2026 The goARRG Authors.

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

#include <stddef.h>
#include <stdint.h>

#include "vkm/std/string.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/utility.hpp"

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "device/device.hpp"
#include "device/vma/vma.hpp"

namespace vkm::vk {
struct context {
	device::instance* instance;
	vkm::std::string<char> name;
	uint32_t queueFamily;
	VkQueue vkQueue;
	VmaPool vmaPool;

	uint32_t frameID = 0;

	struct {
		uint64_t pendingValue;
		VkSemaphore vkSemaphore;
	} semaphore;
	struct frame {
		vkm::std::string<char> name;
		uint64_t pendingSemaphoreValue;

		VkCommandPool vkCommandPool;
		size_t acquiredCommandBuffers;
		size_t submittedCommandBuffers;
		vkm::std::vector<VkCommandBuffer> commandBuffers;

		vkm::std::vector<VkSemaphore> pendingBinarySemaphores;
		vkm::std::vector<vkm::std::pair<VkBuffer, VmaAllocation>> pendingScratchBuffers;
		vkm::std::vector<VkSemaphoreSubmitInfo> pendingWaitSemaphores;
		vkm::std::vector<VkSemaphoreSubmitInfo> pendingSignalSemaphores;

		vkm::std::vector<vkm_destroyer> pendingDestroyers;
	};
	vkm::std::vector<frame> frames;

	[[nodiscard]] vkm_context handle() noexcept { return reinterpret_cast<vkm_context>(this); }
	[[nodiscard]] static context* fromHandle(vkm_context handle) noexcept { return reinterpret_cast<context*>(handle); }
};
}  // namespace vkm::vk
