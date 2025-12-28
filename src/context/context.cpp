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

#include <stdint.h>
#include <stddef.h>
#include <new>

#include "vkm/std/string.hpp"
#include "vkm/std/utility.hpp"
#include "vkm/std/stdlib.hpp"
#include "vkm/std/array.hpp"
#include "vkm/std/vector.hpp"

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "vklog.hpp"
#include "reflect_const.hpp"
#include "device/device.hpp"
#include "device/vma/vma.hpp"
#include "context/context.hpp"
#include "swapchain/swapchain.hpp"

VKM_FN void vkm_createContext(vkm_device instanceHandle, vkm_string name, vkm_contextCreateInfo info, vkm_context* ctxHandle) {
	auto* instance = ::vkm::vk::device::instance::fromHandle(instanceHandle);
	auto* ctx = new (::std::nothrow)::vkm::vk::context();

	ctx->instance = instance;
	if (name.len != 0 && name.ptr != nullptr) {
		vkm::std::stringbuilder builder;
		builder << name << "_context_" << info.queueFamily << "_" << info.queueIndex;
		ctx->name = builder.str();
	} else {
		vkm::std::stringbuilder builder;
		builder << "context_" << info.queueFamily << "_" << info.queueIndex;
		ctx->name = builder.str();
	}
	ctx->queueFamily = info.queueFamily;
	VK_PROC_DEVICE(instance, vkGetDeviceQueue)(instance->vkDevice, info.queueFamily, info.queueIndex, &ctx->vkQueue);

	{
		ctx->semaphore.pendingValue = 0;
		vkm_createTimelineSemaphore(instanceHandle, ctx->name.vkm_string(), ctx->semaphore.pendingValue, &ctx->semaphore.vkSemaphore);
	}
	{
		uint32_t memTypeIndex;
		{
			VkBufferCreateInfo bufferCreateInfo = {};
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			// bufferCreateInfo.size = 1024;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

			VmaAllocationCreateInfo allocCreateInfo = {};
			allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			allocCreateInfo.memoryTypeBits = instance->vma.noBARMemoryTypeBits;

			const VkResult ret = vmaFindMemoryTypeIndexForBufferInfo(
				instance->vma.allocator, &bufferCreateInfo, &allocCreateInfo, &memTypeIndex);
			if (ret != VK_SUCCESS) {
				vkm::fatal(vkm::std::sourceLocation::current(),
						   "Failed to find host memory type for scratch buffers: %s", vkm::vk::reflect::toString(ret).cStr());
			}
		}

		VmaPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.memoryTypeIndex = memTypeIndex;
		poolCreateInfo.flags = VMA_POOL_CREATE_IGNORE_BUFFER_IMAGE_GRANULARITY_BIT | VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT;
		poolCreateInfo.minBlockCount = 0;
		poolCreateInfo.maxBlockCount = 0;
		poolCreateInfo.blockSize = 0;

		const VkResult ret = vmaCreatePool(instance->vma.allocator, &poolCreateInfo, &ctx->vmaPool);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create VmaPool: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
		vkm::std::debugRun([=]() {
			vkm::std::stringbuilder builder;
			builder.write(ctx->name).write("_hostScratchPool");
			vmaSetPoolName(instance->vma.allocator, ctx->vmaPool, builder.cStr());
		});
	}
	{
		ctx->frames.resize(vkm::std::max(1u, info.maxPendingFrames));
		for (size_t i = 0; i < ctx->frames.size(); i++) {
			auto& frame = ctx->frames[i];
			{
				vkm::std::stringbuilder builder;
				builder << ctx->name << "_frame_" << i;
				frame.name = builder.str();
				frame.pendingSemaphoreValue = frame.acquiredCommandBuffers = frame.submittedCommandBuffers = 0;
			}
			{
				const VkCommandPoolCreateInfo poolInfo = {
					.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.pNext = info.commandPoolCreateInfo.pNext,
					.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | info.commandPoolCreateInfo.flags,
					.queueFamilyIndex = info.queueFamily,
				};
				const VkResult ret = VK_PROC_DEVICE(instance, vkCreateCommandPool)(
					instance->vkDevice, &poolInfo, nullptr, &frame.vkCommandPool);
				if (ret != VK_SUCCESS) {
					vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create commandpool: %s",
							   vkm::vk::reflect::toString(ret).cStr());
				}
				vkm::std::debugRun([&]() {
					vkm::std::stringbuilder builder;
					builder << ctx->name << "_cmdPool_" << i;
					vkm::vk::debugLabel(instance->vkDevice, frame.vkCommandPool, builder.cStr());
				});
			}
		}
	}

	*ctxHandle = ctx->handle();
}
VKM_FN void vkm_destroyContext(vkm_context ctxHandle) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	vkm_semaphore_timeline_wait(ctx->instance->handle(), ctx->semaphore.vkSemaphore, ctx->semaphore.pendingValue);
	VK_PROC_DEVICE(ctx->instance, vkDestroySemaphore)(ctx->instance->vkDevice, ctx->semaphore.vkSemaphore, nullptr);
	for (auto& frame : ctx->frames) {
		for (auto& d : frame.pendingDestroyers) {
			d.fn(d.data);
		}
		for (VkSemaphore s : frame.pendingBinarySemaphores) {
			ctx->instance->syncObjectManager.releaseBinarySemaphore(s);
		}
		for (auto& b : frame.pendingScratchBuffers) {
			vmaUnmapMemory(ctx->instance->vma.allocator, b.second);
			vmaDestroyBuffer(ctx->instance->vma.allocator, b.first, b.second);
		}
		VK_PROC_DEVICE(ctx->instance, vkFreeCommandBuffers)(
			ctx->instance->vkDevice, frame.vkCommandPool, frame.commandBuffers.size(), frame.commandBuffers.get());
		VK_PROC_DEVICE(ctx->instance, vkDestroyCommandPool)(ctx->instance->vkDevice, frame.vkCommandPool, nullptr);
	}
	vmaDestroyPool(ctx->instance->vma.allocator, ctx->vmaPool);
	delete ctx;
}
VKM_FN VkBool32 vkm_context_getSwapchainPresentationSupport(vkm_context ctxHandle, vkm_swapchain swapchainHandle) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	auto* swapchain = vkm::vk::swapchain::fromHandle(swapchainHandle);
	VkBool32 presentSupport = VK_FALSE;
	const VkResult ret = VKM_VKFN(vkGetPhysicalDeviceSurfaceSupportKHR)(
		ctx->instance->vkPhysicalDevice, ctx->queueFamily, swapchain->vkSurface, &presentSupport);
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed to query presentation support: %s",
				   vkm::vk::reflect::toString(ret).cStr());
	}
	return presentSupport;
}
VKM_FN void vkm_context_begin(vkm_context ctxHandle, vkm_string name) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	auto& frame = ctx->frames[ctx->frameID];

	vkm_semaphore_timeline_wait(ctx->instance->handle(), ctx->semaphore.vkSemaphore, frame.pendingSemaphoreValue);
	{
		for (auto& d : frame.pendingDestroyers) {
			d.fn(d.data);
		}
		frame.pendingDestroyers.resize(0);
	}
	{
		for (VkSemaphore s : frame.pendingBinarySemaphores) {
			ctx->instance->syncObjectManager.releaseBinarySemaphore(s);
		}
		frame.pendingBinarySemaphores.resize(0);
	}
	{
		for (auto& b : frame.pendingScratchBuffers) {
			vmaUnmapMemory(ctx->instance->vma.allocator, b.second);
			vmaDestroyBuffer(ctx->instance->vma.allocator, b.first, b.second);
		}
		frame.pendingScratchBuffers.resize(0);
	}
	{
		{
			const VkResult ret = VK_PROC_DEVICE(ctx->instance, vkResetCommandPool)(ctx->instance->vkDevice, frame.vkCommandPool, 0);
			if (ret != VK_SUCCESS) {
				vkm::fatal(vkm::std::sourceLocation::current(), "Failed to reset command pool: %s",
						   vkm::vk::reflect::toString(ret).cStr());
			}
		}
		if (frame.acquiredCommandBuffers != frame.submittedCommandBuffers) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Acquired %zu command buffers but submitted %zu",
					   frame.acquiredCommandBuffers, frame.submittedCommandBuffers);
		}
		frame.acquiredCommandBuffers = frame.submittedCommandBuffers = 0;
	}

	vkm::std::debugRun([&]() {
		if (name.len != 0 && name.ptr != nullptr) {
			vkm::std::stringbuilder builder;
			builder << frame.name << "_" << name;
			vkm::vk::debugLabelBegin(ctx->vkQueue, builder.cStr());
		} else {
			vkm::vk::debugLabelBegin(ctx->vkQueue, frame.name.cStr());
		}
	});
}
VKM_FN void vkm_context_queueDestroyer(vkm_context ctxHandle, vkm_destroyer destroyer) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	auto& frame = ctx->frames[ctx->frameID];
	frame.pendingDestroyers.pushBack(destroyer);
}
VKM_FN void vkm_context_createScratchHostBuffer(vkm_context ctxHandle, vkm_string name, VkBufferCreateInfo info, vkm_hostBuffer* b) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	auto& frame = ctx->frames[ctx->frameID];
	auto* instance = ctx->instance;

	{
		const VmaAllocationCreateInfo allocCreateInfo = {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
			.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			.pool = ctx->vmaPool,
		};
		VkResult ret = vmaCreateBuffer(instance->vma.allocator, &info, &allocCreateInfo, &b->vkBuffer,
									   reinterpret_cast<VmaAllocation*>(&b->allocation), nullptr);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create buffer: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
		ret = vmaMapMemory(instance->vma.allocator, reinterpret_cast<VmaAllocation>(b->allocation), &b->ptr);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to map buffer: %s", vkm::vk::reflect::toString(ret).cStr());
		}
		frame.pendingScratchBuffers.pushBack(vkm::std::pair{b->vkBuffer, reinterpret_cast<VmaAllocation>(b->allocation)});
	}
	vkm::std::debugRun([=]() {
		vkm::std::stringbuilder builder;
		builder.write(name).write("_scratchHostBuffer");
		vkm::vk::debugLabel(instance->vkDevice, b->vkBuffer, builder.cStr());

		builder.write("_allocation");
		vmaSetAllocationName(instance->vma.allocator, reinterpret_cast<VmaAllocation>(b->allocation), builder.cStr());
	});
}
VKM_FN void vkm_context_acquireSwapchain(vkm_context ctxHandle, size_t count, vkm_swapchain_acquireInfo* infos) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	auto& frame = ctx->frames[ctx->frameID];

	for (size_t i = 0; i < count; i++) {
		auto info = infos[i];
		if (info.stage == 0) {
			info.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		}
		auto* swapchain = vkm::vk::swapchain::fromHandle(info.swapchain);
		VkSemaphore semaphore = ctx->instance->syncObjectManager.acquireBinarySemaphore();
		{
			frame.pendingBinarySemaphores.pushBack(semaphore);
			frame.pendingWaitSemaphores.pushBack(VkSemaphoreSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = semaphore,
				.stageMask = info.stage,
			});
		}
		*info.pResult = swapchain->acquire(semaphore, info.pImage);
		if (*info.pResult >= 0) {
			vkm::std::debugRun([&]() {
				vkm::std::stringbuilder builder;
				builder << swapchain->name << "_semaphoreBinary_surfaceAcquire_frame_" << ctx->frameID;
				vkm::vk::debugLabel(ctx->instance->vkDevice, semaphore, builder.cStr());
			});
		}
	}
}
VKM_FN void vkm_context_beginCommandBuffer(vkm_context ctxHandle, vkm_string name,
										   vkm_context_commandBufferBeginInfo info, VkCommandBuffer* cb) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	auto& frame = ctx->frames[ctx->frameID];

	if (frame.acquiredCommandBuffers != frame.submittedCommandBuffers) {
		vkm::fatal("Cannot begin another command buffer until after ending the current one");
	}

	if (frame.commandBuffers.size() > frame.acquiredCommandBuffers) {
		*cb = frame.commandBuffers[frame.acquiredCommandBuffers];
	} else {
		const VkCommandBufferAllocateInfo layoutCmdAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = frame.vkCommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		const VkResult ret = VK_PROC_DEVICE(ctx->instance, vkAllocateCommandBuffers)(
			ctx->instance->vkDevice, &layoutCmdAllocateInfo, cb);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to create command buffer: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
		frame.commandBuffers.pushBack(*cb);
	}
	{
		const VkCommandBufferBeginInfo beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = info.pNext,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | info.flags,
		};
		const VkResult ret = VK_PROC_DEVICE(ctx->instance, vkBeginCommandBuffer)(*cb, &beginInfo);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to begin command buffer: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
	}
	vkm::std::debugRun([&]() {
		vkm::std::stringbuilder builder;
		if (name.len != 0 && name.ptr != nullptr) {
			builder << name;
		} else {
			builder << frame.name << "_commandBuffer_" << frame.acquiredCommandBuffers;
		}
		vkm::vk::debugLabelBegin(*cb, builder.cStr());
	});
	frame.acquiredCommandBuffers += 1;
}
VKM_FN void vkm_context_endCommandBuffer(vkm_context ctxHandle, vkm_context_commandBufferEndInfo info) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	auto& frame = ctx->frames[ctx->frameID];
	VkCommandBuffer cb = frame.commandBuffers[frame.submittedCommandBuffers];

	if (frame.acquiredCommandBuffers == frame.submittedCommandBuffers) {
		vkm::fatal("No active command buffer to end");
	}

	vkm::vk::debugLabelEnd(cb);
	{
		const VkResult ret = VK_PROC_DEVICE(ctx->instance, vkEndCommandBuffer)(cb);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to end command buffer: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
	}
	{
		vkm::std::array commandbuffers = {
			VkCommandBufferSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
				.pNext = info.commandBufferSubmitInfo.pNext,
				.commandBuffer = cb,
			},
		};
		VkSubmitInfo2 submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.pNext = info.submitInfo.pNext,
			.flags = info.submitInfo.flags,

			.waitSemaphoreInfoCount = info.submitInfo.numWaitSemaphores,
			.pWaitSemaphoreInfos = info.submitInfo.pWaitSemaphores,

			.commandBufferInfoCount = commandbuffers.size(),
			.pCommandBufferInfos = commandbuffers.get(),
		};
		if (frame.pendingWaitSemaphores.size() > 0) {
			frame.pendingWaitSemaphores.pushBack(info.submitInfo.numWaitSemaphores, info.submitInfo.pWaitSemaphores);
			submitInfo.waitSemaphoreInfoCount = frame.pendingWaitSemaphores.size();
			submitInfo.pWaitSemaphoreInfos = frame.pendingWaitSemaphores.get();
		}
		{
			for (size_t i = 0; i < info.numPrsentInfos; i++) {
				auto present = info.pPresentInfos[i];
				auto* swapchain = vkm::vk::swapchain::fromHandle(present.swapchain);
				if (present.stage == 0) {
					frame.pendingSignalSemaphores.pushBack(VkSemaphoreSubmitInfo{
						.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
						.semaphore = swapchain->semaphore(),
						.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
					});
				} else {
					frame.pendingSignalSemaphores.pushBack(VkSemaphoreSubmitInfo{
						.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
						.semaphore = swapchain->semaphore(),
						.stageMask = present.stage,
					});
				}
			}
			frame.pendingSignalSemaphores.pushBack(VkSemaphoreSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = ctx->semaphore.vkSemaphore,
				.value = ++ctx->semaphore.pendingValue,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			});
			frame.pendingSignalSemaphores.pushBack(info.submitInfo.numSignalSemaphores, info.submitInfo.pSignalSemaphores);
			submitInfo.signalSemaphoreInfoCount = frame.pendingSignalSemaphores.size();
			submitInfo.pSignalSemaphoreInfos = frame.pendingSignalSemaphores.get();
		}
		const VkResult ret = VK_PROC_DEVICE(ctx->instance, vkQueueSubmit2)(ctx->vkQueue, 1, &submitInfo, VK_NULL_HANDLE);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to submit %s: %s", frame.name.cStr(),
					   vkm::vk::reflect::toString(ret).cStr());
		}
	}
	frame.pendingWaitSemaphores.resize(0);
	frame.pendingSignalSemaphores.resize(0);
	frame.submittedCommandBuffers += 1;
	for (size_t i = 0; i < info.numPrsentInfos; i++) {
		auto present = info.pPresentInfos[i];
		auto* swapchain = vkm::vk::swapchain::fromHandle(present.swapchain);
		*present.pResult = swapchain->present(ctx->vkQueue);
	}
}
VKM_FN void vkm_context_end(vkm_context ctxHandle) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	auto& frame = ctx->frames[ctx->frameID];

	if (frame.acquiredCommandBuffers != frame.submittedCommandBuffers) {
		vkm::fatal("Cannot end context before ending active command buffer");
	}

	vkm::std::debugRun([&]() { vkm::vk::debugLabelEnd(ctx->vkQueue); });
	frame.pendingSemaphoreValue = ctx->semaphore.pendingValue;
	ctx->frameID = (ctx->frameID + 1) % ctx->frames.size();
}
VKM_FN void vkm_context_wait(vkm_context ctxHandle) {
	auto* ctx = ::vkm::vk::context::fromHandle(ctxHandle);
	vkm_semaphore_timeline_wait(ctx->instance->handle(), ctx->semaphore.vkSemaphore, ctx->semaphore.pendingValue);
}
