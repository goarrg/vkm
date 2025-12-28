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

#include <stddef.h>
#include <stdint.h>

#include "vkm/std/vector.hpp"
#include "vkm/std/utility.hpp"

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "reflect_const.hpp"
#include "initializer/initializer.hpp"

// NOLINTNEXTLINE(readability-make-member-function-const)
[[nodiscard]] bool vkm::vk::initializer::initializer::checkQueueConfig() noexcept {
	{
		bool ok = false;
		if (this->graphicsQueueRequirements.max > 0) {
			ok = true;
		}
		if (this->computeQueueRequirements.max > 0) {
			ok = true;
		}
		if (this->transferQueueRequirements.max > 0) {
			ok = true;
		}
		if (!ok) {
			vkm::ePrintf("Device selector has no queues required");
			return false;
		}
	}

	return true;
}
[[nodiscard]] bool vkm::vk::initializer::initializer::findQueues(VkPhysicalDevice vkPhysicalDevice) noexcept {
	uint32_t haveQueueFamilies = 0;
	VK_PROC(vkGetPhysicalDeviceQueueFamilyProperties)
	(vkPhysicalDevice, &haveQueueFamilies, nullptr);
	vkm::std::vector<VkQueueFamilyProperties> queueFamilies(haveQueueFamilies);
	VK_PROC(vkGetPhysicalDeviceQueueFamilyProperties)
	(vkPhysicalDevice, &haveQueueFamilies, queueFamilies.get());

	queueCreateInfos.resize(0);

	auto findQueue = [&](VkQueueFlags wantFlags, VkQueueFlags dontWantFlags, queueRequirements& requirements) -> bool {
		requirements.createInfo.count = 0;
		if (requirements.max == 0) {
			return true;
		}
		for (uint32_t i = 0; i < queueFamilies.size(); i++) {
			if (vkm::std::cmpBitFlags(queueFamilies[i].queueFlags, wantFlags, dontWantFlags)
				&& (queueFamilies[i].queueCount >= requirements.min)) {
				requirements.createInfo.family = i;
				requirements.createInfo.count = vkm::std::clamp(
					queueFamilies[i].queueCount, requirements.min, requirements.max);
				auto info = VkDeviceQueueCreateInfo{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.flags = requirements.createInfo.flags,
					.queueFamilyIndex = requirements.createInfo.family,
					.queueCount = requirements.createInfo.count,
					.pQueuePriorities = requirements.createInfo.priorities.get(),
				};
				if (requirements.createInfo.pNext.size() > 0) {
					info.pNext = requirements.createInfo.pNext.first().get();
				}
				this->queueCreateInfos.pushBack(info);
				return true;
			}
		}
		return false;
	};

	if (!findQueue(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, this->graphicsQueueRequirements)) {
		this->appendRejectReason(
			"Failed to find graphics queue family with at least [%d] queues", this->graphicsQueueRequirements.min);
		return false;
	}
	if (!findQueue(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, this->computeQueueRequirements)) {
		this->appendRejectReason(
			"Failed to find compute queue family with at least [%d] queues", this->computeQueueRequirements.min);
		return false;
	}
	if (!findQueue(VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, this->transferQueueRequirements)) {
		this->appendRejectReason(
			"Failed to find transfer queue family with at least [%d] queues", this->transferQueueRequirements.min);
		return false;
	}

	bool ok = true;
	for (VkSurfaceKHR s : targetSurfaces) {
		VkBool32 presentSupport = VK_FALSE;
		for (auto q : queueCreateInfos) {
			const VkResult ret = VKM_VKFN(vkGetPhysicalDeviceSurfaceSupportKHR)(vkPhysicalDevice, q.queueFamilyIndex, s, &presentSupport);
			if (ret != VK_SUCCESS) {
				appendRejectReason("Failed to query presentation support: %s", vkm::vk::reflect::toString(ret).cStr());
				return false;
			}
			if (presentSupport == VK_TRUE) {
				break;
			}
		}
		if (presentSupport == VK_FALSE) {
			ok = false;
			appendRejectReason("Unable to present to surface: 0x%X", s);
		}
	}

	return ok;
}
