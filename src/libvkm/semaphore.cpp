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

#include "vkm/std/stdlib.hpp"
#include "vkm/std/array.hpp"
#include "vkm/std/time.hpp"
#include "vkm/std/string.hpp"

#include "vkm.hpp"
#include "vklog.hpp"
#include "reflect_const.hpp"
#include "device/device.hpp"

extern "C" {
VKM_FN void vkm_createTimelineSemaphore(vkm_device instanceHandle, vkm_string name, uint64_t value, VkSemaphore* semaphore) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	{
		const VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue = value,
		};
		const VkSemaphoreCreateInfo semaphoreInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &semaphoreTypeInfo,
		};
		const VkResult ret = VK_PROC_DEVICE(instance, vkCreateSemaphore)(instance->vkDevice, &semaphoreInfo, nullptr, semaphore);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create semaphore: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
	}
	vkm::std::debugRun([=]() {
		vkm::std::stringbuilder builder;
		builder.write(name).write("_semaphoreTimeline");
		vkm::vk::debugLabel(instance->vkDevice, *semaphore, builder.cStr());
	});
}
VKM_FN void vkm_destroyTimelineSemaphore(vkm_device instanceHandle, VkSemaphore semaphore) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	VK_PROC_DEVICE(instance, vkDestroySemaphore)(instance->vkDevice, semaphore, nullptr);
}
VKM_FN void vkm_semaphore_timeline_signal(vkm_device instanceHandle, VkSemaphore semaphore, uint64_t value) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	const VkSemaphoreSignalInfo signalInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
		.semaphore = semaphore,
		.value = value,
	};
	const VkResult ret = VK_PROC_DEVICE(instance, vkSignalSemaphore)(instance->vkDevice, &signalInfo);
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed waiting on semaphore: %s",
				   vkm::vk::reflect::toString(ret).cStr());
	}
}
VKM_FN void vkm_semaphore_timeline_wait(vkm_device instanceHandle, VkSemaphore semaphore, uint64_t value) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);

	VkSemaphoreWaitInfo waitInfo = {};
	waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;

	vkm::std::array semaphores = {
		semaphore,
	};
	vkm::std::array<uint64_t, semaphores.size()> values = {
		value,
	};
	waitInfo.semaphoreCount = semaphores.size();
	waitInfo.pSemaphores = semaphores.get();
	waitInfo.pValues = values.get();

	const VkResult ret = VK_PROC_DEVICE(instance, vkWaitSemaphores)(instance->vkDevice, &waitInfo, vkm::std::time::second);
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed waiting on semaphore: %s",
				   vkm::vk::reflect::toString(ret).cStr());
	}
}
VKM_FN uint64_t vkm_semaphore_timeline_getValue(vkm_device instanceHandle, VkSemaphore semaphore) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	uint64_t value = 0;
	const VkResult ret = VK_PROC_DEVICE(instance, vkGetSemaphoreCounterValue)(instance->vkDevice, semaphore, &value);
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed getting semaphore value: %s",
				   vkm::vk::reflect::toString(ret).cStr());
	}
	return value;
}
}
