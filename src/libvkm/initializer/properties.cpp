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

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "initializer/initializer.hpp"

[[nodiscard]] bool vkm::vk::initializer::initializer::findProperties(VkPhysicalDevice vkPhysicalDevice) noexcept {
	VkPhysicalDeviceProperties properties = {};
	VK_PROC(vkGetPhysicalDeviceProperties)(vkPhysicalDevice, &properties);

	if (properties.apiVersion < this->requiredAPI) {
		this->appendRejectReason("Device API %d.%d < required API %d.%d",  //
								 VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion),
								 VK_API_VERSION_MAJOR(this->requiredAPI), VK_API_VERSION_MINOR(this->requiredAPI));
		return false;
	}

	return true;
}
