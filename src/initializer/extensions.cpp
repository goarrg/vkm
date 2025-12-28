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

#include "vkm/std/vector.hpp"
#include "vkm/std/stdlib.hpp"

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "reflect_const.hpp"
#include "initializer/initializer.hpp"

[[nodiscard]] bool vkm::vk::initializer::initializer::findExtensions(VkPhysicalDevice vkPhysicalDevice) noexcept {
	uint32_t sz;
	VkResult ret = VK_PROC(vkEnumerateDeviceExtensionProperties)(vkPhysicalDevice, nullptr, &sz, nullptr);
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed to get device extension list: %s",
				   vkm::vk::reflect::toString(ret).cStr());
	}
	vkm::std::vector<VkExtensionProperties> properties(sz);
	ret = VK_PROC(vkEnumerateDeviceExtensionProperties)(vkPhysicalDevice, nullptr, &sz, properties.get());
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed to get device extension list: %s",
				   vkm::vk::reflect::toString(ret).cStr());
	}
	enabledDeviceExtensions.resize(0);

	bool ok = true;
	for (const auto& require : requiredDeviceExtensions) {
		bool found = false;
		for (auto have : properties) {
			if (require == have.extensionName) {
				found = true;
				break;
			}
		}

		if (found) {
			enabledDeviceExtensions.pushBack(require);
		} else {
			this->appendRejectReason("Failed to find required extension: %s", require.cStr());
			ok = false;
		}
	}
	for (const auto& optional : optionalDeviceExtensions) {
		bool found = false;
		for (auto have : properties) {
			if (optional == have.extensionName) {
				found = true;
				break;
			}
		}

		if (found) {
			enabledDeviceExtensions.pushBack(optional);
		}
	}

	this->enabledDeviceExtensions.sortComptact();
	return ok;
}
