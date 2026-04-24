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

#include "device/sync/sync.hpp"

#include "vkm/std/stdlib.hpp"

#include "vkm.hpp"
#include "vklog.hpp"
#include "reflect_const.hpp"
#include "device/device.hpp"

namespace vkm::vk::device {
syncObjectManager::syncObjectManager(struct instance* instance) noexcept : instance(instance) {}

void syncObjectManager::clear() {
	for (VkSemaphore s : this->freeSemaphores) {
		VK_PROC_DEVICE(this->instance, vkDestroySemaphore)(this->instance->vkDevice, s, nullptr);
	}
	for (VkFence f : this->freeFences) {
		VK_PROC_DEVICE(this->instance, vkDestroyFence)(this->instance->vkDevice, f, nullptr);
	}
	this->freeSemaphores.resize(0);
	this->freeFences.resize(0);
}

VkSemaphore syncObjectManager::acquireBinarySemaphore() noexcept {
	if (this->freeSemaphores.size() > 0) {
		return this->freeSemaphores.dequeueBack();
	}
	VkSemaphore s;
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	const VkResult ret = VK_PROC_DEVICE(this->instance, vkCreateSemaphore)(this->instance->vkDevice, &semaphoreInfo, nullptr, &s);
	if (ret != VK_SUCCESS) {
		vkm::fatal(
			vkm::std::sourceLocation::current(), "Failed to create semaphore: %s", vkm::vk::reflect::toString(ret).cStr());
	}
	vkm::vk::debugLabel(instance->vkDevice, s, "semaphoreBinary_acquired");
	return s;
}
void syncObjectManager::releaseBinarySemaphore(VkSemaphore s) noexcept {
	this->freeSemaphores.pushBack(s);
	vkm::vk::debugLabel(instance->vkDevice, s, "semaphoreBinary_released");
}

VkFence syncObjectManager::acquireFence(bool signal) noexcept {
	if (this->freeFences.size() > 0) {
		VkFence f = this->freeFences.dequeueBack();
		if (!signal) {
			const VkResult ret = VK_PROC_DEVICE(this->instance, vkResetFences)(this->instance->vkDevice, 1, &f);
			if (ret != VK_SUCCESS) {
				vkm::fatal(vkm::std::sourceLocation::current(), "Failed to reset fence: %s",
						   vkm::vk::reflect::toString(ret).cStr());
			}
		}
		return f;
	}

	VkFence f;
	VkFenceCreateInfo fenceInfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	if (signal) {
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	}
	const VkResult ret = VK_PROC_DEVICE(this->instance, vkCreateFence)(this->instance->vkDevice, &fenceInfo, nullptr, &f);
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create fence: %s", vkm::vk::reflect::toString(ret).cStr());
	}
	vkm::vk::debugLabel(instance->vkDevice, f, "fence_acquired");
	return f;
}
void syncObjectManager::releaseFence(VkFence f) noexcept {
	const VkResult ret = VK_PROC_DEVICE(this->instance, vkWaitForFences)(this->instance->vkDevice, 1, &f, VK_TRUE, 0);
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Cannot release fence: %s", vkm::vk::reflect::toString(ret).cStr());
	}
	this->freeFences.pushBack(f);
	vkm::vk::debugLabel(instance->vkDevice, f, "fence_released");
}
}  // namespace vkm::vk::device
