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
#include <stddef.h>

#include "vkm/std/algorithm.hpp"
#include "vkm/std/utility.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/stdlib.hpp"

#include "vkm.hpp"
#include "reflect_const.hpp"
#include "device/device.hpp"

namespace vkm::vk::device {
void setupProperties(vkm::vk::device::instance* device) noexcept {
	VkPhysicalDeviceVulkan13Properties device13Properties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES,
	};
	VkPhysicalDeviceVulkan12Properties device12Properties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
		.pNext = &device13Properties,
	};
	VkPhysicalDeviceVulkan11Properties device11Properties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
		.pNext = &device12Properties,
	};
	VkPhysicalDeviceProperties2 deviceProperties2 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = &device11Properties,
	};
	VK_PROC(vkGetPhysicalDeviceProperties2)(device->vkPhysicalDevice, &deviceProperties2);

	{
		auto properties = deviceProperties2.properties;

		// uuid
		{
			uint32_t numDevices = 0;
			VkResult ret = VK_PROC(vkEnumeratePhysicalDevices)(vkm::vkInstance(), &numDevices, nullptr);
			if (ret != VK_SUCCESS) {
				vkm::fatal(vkm::std::sourceLocation::current(), "Failed to get list of GPU devices: %s",
						   vkm::vk::reflect::toString(ret).cStr());
			}
			if (numDevices == 0) {
				vkm::fatal(vkm::std::sourceLocation::current(), "Failed to get list of GPU devices: List is empty");
			}
			if (numDevices >= UINT16_MAX) {
				vkm::fatal(vkm::std::sourceLocation::current(),
						   "Number of vulkan devices overflows uint16_t this should never happen");
			}
			vkm::std::vector<VkPhysicalDevice> devices(numDevices);
			ret = VK_PROC(vkEnumeratePhysicalDevices)(vkm::vkInstance(), &numDevices, devices.get());
			if (ret != VK_SUCCESS) {
				vkm::fatal(vkm::std::sourceLocation::current(), "Failed to get list of GPU devices: %s",
						   vkm::vk::reflect::toString(ret).cStr());
			}
			const size_t i = vkm::std::linearSearch(
				numDevices, [&](size_t i) -> bool { return devices[i] == device->vkPhysicalDevice; });
			if (i == numDevices) {
				vkm::fatal(vkm::std::sourceLocation::current(),
						   "VkPhysicalDevice given was either lost or not created with the same vkInstance");
			}
			vkm::vk::device::generateUUID(properties, i, &device->properties.uuid);
		}

		device->properties.vendorID = properties.vendorID;
		device->properties.deviceID = properties.deviceID;
		device->properties.driverVersion = properties.driverVersion;
		device->properties.api = properties.apiVersion;

		// compute Properties
		{
			device->properties.compute.subgroupSize = device11Properties.subgroupSize;	//
		}
	}

	{
		const VkPhysicalDeviceLimits device10Proprties = deviceProperties2.properties.limits;
		auto* limits = &device->properties.limits;

		{
			limits->minLineWidth = device10Proprties.lineWidthRange[0];
			limits->maxLineWidth = device10Proprties.lineWidthRange[1];

			limits->minPointSize = device10Proprties.pointSizeRange[0];
			limits->maxPointSize = device10Proprties.pointSizeRange[1];
		}

		// global limits
		{
			limits->global.maxAllocationSize = device11Properties.maxMemoryAllocationSize;
			limits->global.maxMemoryAllocationCount = device10Proprties.maxMemoryAllocationCount;
			limits->global.maxSamplerAllocationCount = device10Proprties.maxSamplerAllocationCount;
		}

		// per descriptor limits
		{
			limits->perDescriptor.maxImageDimension1D = device10Proprties.maxImageDimension1D;
			limits->perDescriptor.maxImageDimension2D = device10Proprties.maxImageDimension2D;
			limits->perDescriptor.maxImageDimension3D = device10Proprties.maxImageDimension3D;
			limits->perDescriptor.maxImageDimensionCube = device10Proprties.maxImageDimensionCube;
			limits->perDescriptor.maxImageArrayLayers = device10Proprties.maxImageArrayLayers;

			limits->perDescriptor.maxSamplerAnisotropy = device10Proprties.maxSamplerAnisotropy;
			limits->perDescriptor.maxUBOSize = device10Proprties.maxUniformBufferRange;
			limits->perDescriptor.maxSBOSize = device10Proprties.maxStorageBufferRange;
		}

		// per stage limits
		{
			limits->perStage.maxSamplerCount = device10Proprties.maxPerStageDescriptorSamplers;
			limits->perStage.maxSampledImageCount = device10Proprties.maxPerStageDescriptorSampledImages;
			limits->perStage.maxCombinedImageSamplerCount = vkm::std::min(
				device10Proprties.maxPerStageDescriptorSamplers, device10Proprties.maxPerStageDescriptorSampledImages);
			limits->perStage.maxStorageImageCount = device10Proprties.maxPerStageDescriptorStorageImages;
			limits->perStage.maxUBOCount = device10Proprties.maxPerStageDescriptorUniformBuffers;
			limits->perStage.maxSBOCount = device10Proprties.maxPerStageDescriptorStorageBuffers;
			limits->perStage.maxResourceCount = device10Proprties.maxPerStageResources;
		}

		// per pipeline limits
		{
			limits->perPipeline.maxSamplerCount = device10Proprties.maxDescriptorSetSamplers;
			limits->perPipeline.maxSampledImageCount = device10Proprties.maxDescriptorSetSampledImages;
			limits->perPipeline.maxCombinedImageSamplerCount = vkm::std::min(
				device10Proprties.maxDescriptorSetSamplers, device10Proprties.maxDescriptorSetSampledImages);
			limits->perPipeline.maxStorageImageCount = device10Proprties.maxDescriptorSetStorageImages;

			limits->perPipeline.maxUBOCount = device10Proprties.maxDescriptorSetUniformBuffers;
			limits->perPipeline.maxSBOCount = device10Proprties.maxDescriptorSetStorageBuffers;

			limits->perPipeline.maxBoundDescriptorSets = device10Proprties.maxBoundDescriptorSets;
			limits->perPipeline.maxPushConstantsSize = device10Proprties.maxPushConstantsSize;
		}

		// compute Limits
		{
			limits->compute.maxDispatchSize = VkExtent3D{
				device10Proprties.maxComputeWorkGroupCount[0],
				device10Proprties.maxComputeWorkGroupCount[1],
				device10Proprties.maxComputeWorkGroupCount[2],
			};
			limits->compute.maxWorkgroupSize = VkExtent3D{
				device10Proprties.maxComputeWorkGroupSize[0],
				device10Proprties.maxComputeWorkGroupSize[1],
				device10Proprties.maxComputeWorkGroupSize[2],
			};

			limits->compute.workgroup = {
				.maxInvocations = device10Proprties.maxComputeWorkGroupInvocations,
				.maxSubgroupCount = device13Properties.maxComputeWorkgroupSubgroups,
			};

			limits->compute.minSubgroupSize = device13Properties.minSubgroupSize;
			limits->compute.maxSubgroupSize = device13Properties.maxSubgroupSize;
		}
	}
}
}  // namespace vkm::vk::device
