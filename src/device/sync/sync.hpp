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

#include <stdio.h>

#include "vkm/std/vector.hpp"

#include "vkm.hpp"

namespace vkm::vk::device {
struct instance;
class syncObjectManager {
   private:
	struct instance* instance;
	vkm::std::vector<VkSemaphore> freeSemaphores;
	vkm::std::vector<VkFence> freeFences;

   public:
	syncObjectManager() = delete;
	syncObjectManager(const syncObjectManager&) = delete;
	syncObjectManager& operator=(const syncObjectManager&) = delete;

	syncObjectManager(struct instance*) noexcept;
	~syncObjectManager() noexcept = default;

	void clear();

	VkSemaphore acquireBinarySemaphore() noexcept;
	void releaseBinarySemaphore(VkSemaphore) noexcept;

	VkFence acquireFence(bool) noexcept;
	void releaseFence(VkFence) noexcept;
};
}  // namespace vkm::vk::device
