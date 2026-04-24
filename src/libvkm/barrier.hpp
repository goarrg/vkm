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

#include "vkm/std/vector.hpp"

#include "vkm/vkm.h"

namespace vkm::vk {
class barrier {
	vkm::std::vector<VkMemoryBarrier2> memoryBarriers;
	vkm::std::vector<VkBufferMemoryBarrier2> bufferBarriers;
	vkm::std::vector<VkImageMemoryBarrier2> imageBarriers;

   public:
	void submit(VkCommandBuffer) const noexcept {
		VkDependencyInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

		info.memoryBarrierCount = memoryBarriers.size();
		info.pMemoryBarriers = memoryBarriers.get();

		info.bufferMemoryBarrierCount = bufferBarriers.size();
		info.pBufferMemoryBarriers = bufferBarriers.get();

		info.imageMemoryBarrierCount = imageBarriers.size();
		info.pImageMemoryBarriers = imageBarriers.get();

		// VK_PROC_DEVICE(vkCmdPipelineBarrier2)(cb, &info);
	}

	barrier& reset() noexcept {
		memoryBarriers.resize(0);
		bufferBarriers.resize(0);
		imageBarriers.resize(0);
		return *this;
	}

	barrier& memory(VkMemoryBarrier2 memoryBarrier) noexcept {
		memoryBarriers.pushBack(memoryBarrier);
		return *this;
	}
	barrier& write(VkPipelineStageFlags2 stage) noexcept {
		VkMemoryBarrier2 barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		barrier.srcStageMask = barrier.dstStageMask = stage;
		barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
		return memory(barrier);
	}
	barrier& execution(VkPipelineStageFlags2 stage) noexcept {
		VkMemoryBarrier2 barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		barrier.srcStageMask = barrier.dstStageMask = stage;
		return memory(barrier);
	}

	barrier& buffer(VkBufferMemoryBarrier2 bufferBarrier) noexcept {
		bufferBarriers.pushBack(bufferBarrier);
		return *this;
	}

	barrier& image(VkImageMemoryBarrier2 imageBarrier) noexcept {
		imageBarriers.pushBack(imageBarrier);
		return *this;
	}
};
}  // namespace vkm::vk
