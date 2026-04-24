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

#include <stddef.h>
#include <stdint.h>

#include "vkm/std/string.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/utility.hpp"
#include "vkm/std/memory.hpp"
#include "vkm/std/stdlib.hpp"

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "reflect_struct.hpp"

namespace vkm::vk::initializer {
struct featureChain {
	vkm::std::vector<vkm::std::smartPtr<vkm::vk::reflect::vkStructureChain>> allocations;
	VkPhysicalDeviceFeatures2 start{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

	void reset() noexcept {
		this->start = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
		this->allocations.resize(0);
	}

	void append(VkStructureType sType) noexcept {
		if (sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
			for (auto& s : this->allocations) {
				if (s->sType == sType) {
					return;
				}
			}
			auto alloc = vkm::vk::reflect::device::featureStruct::typeOf(sType)->allocate();
			if (this->allocations.size() == 0) {
				this->start.pNext = alloc.get();
			} else {
				this->allocations.last()->pNext = alloc.get();
			}
			this->allocations.pushBack(vkm::std::move(alloc));
		}
	}
	void append(vkm::vk::reflect::vkStructureChain* ptr) noexcept {
		vkm::std::smartPtr<vkm::vk::reflect::structValue> vIn;
		vkm::std::smartPtr<vkm::vk::reflect::structValue> vOut;

		if (ptr->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
			vIn = vkm::vk::reflect::device::featureStruct::valueOf(&reinterpret_cast<VkPhysicalDeviceFeatures2*>(ptr)->features);
			vOut = vkm::vk::reflect::device::featureStruct::valueOf(&this->start.features);
		} else {
			vIn = vkm::vk::reflect::device::featureStruct::valueOf(ptr);
			for (auto& s : this->allocations) {
				if (s->sType == ptr->sType) {
					vOut = vkm::vk::reflect::device::featureStruct::valueOf(s.get());
					break;
				}
			}
			if (vOut.get() == nullptr) {
				auto alloc = vIn->type->allocate();
				vOut = vkm::vk::reflect::device::featureStruct::valueOf(alloc.get());
				if (this->allocations.size() == 0) {
					this->start.pNext = alloc.get();
				} else {
					this->allocations.last()->pNext = alloc.get();
				}
				this->allocations.pushBack(vkm::std::move(alloc));
			}
		}

		for (size_t i = 0; i < vIn->numFields(); i++) {
			auto fIn = vIn->field(i);
			auto fOut = vOut->field(i);
			if (fOut.type.id == vkm::vk::reflect::type::vkBool32) {
				*static_cast<VkBool32*>(fOut.ptr) |= *static_cast<VkBool32*>(fIn.ptr);
			}
		}
	}
	void append(VkStructureType sType, size_t numFeatures, size_t* features) {
		vkm::std::smartPtr<vkm::vk::reflect::structValue> v;

		if (sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
			v = vkm::vk::reflect::device::featureStruct::valueOf(&this->start.features);
		} else {
			for (auto& s : this->allocations) {
				if (s->sType == sType) {
					v = vkm::vk::reflect::device::featureStruct::valueOf(s.get());
					break;
				}
			}
			if (v.get() == nullptr) {
				auto alloc = vkm::vk::reflect::device::featureStruct::typeOf(sType)->allocate();
				v = vkm::vk::reflect::device::featureStruct::valueOf(alloc.get());
				if (this->allocations.size() == 0) {
					this->start.pNext = alloc.get();
				} else {
					this->allocations[this->allocations.size() - 1]->pNext = alloc.get();
				}
				this->allocations.pushBack(vkm::std::move(alloc));
			}
		}

		for (size_t i = 0; i < numFeatures; i++) {
			auto f = v->field(features[i]);
			if (f.type.id != vkm::vk::reflect::type::vkBool32) {
				vkm::fatal(vkm::std::sourceLocation::current(), "Tryting to set %s.%s which is not a feature toggle",
						   v->type->name, f.name);
			}
			*static_cast<VkBool32*>(f.ptr) = VK_TRUE;
		}
	}
	void extract(vkm::vk::reflect::vkStructureChain* ptr) noexcept {
		vkm::std::smartPtr<vkm::vk::reflect::structValue> vIn;
		vkm::std::smartPtr<vkm::vk::reflect::structValue> vOut;

		if (ptr->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
			vIn = vkm::vk::reflect::device::featureStruct::valueOf(&this->start.features);
			vOut = vkm::vk::reflect::device::featureStruct::valueOf(&reinterpret_cast<VkPhysicalDeviceFeatures2*>(ptr)->features);
		} else {
			vOut = vkm::vk::reflect::device::featureStruct::valueOf(ptr);
			for (auto& s : this->allocations) {
				if (s->sType == ptr->sType) {
					vIn = vkm::vk::reflect::device::featureStruct::valueOf(s.get());
					break;
				}
			}
		}
		if (vIn.get() == nullptr) {
			for (size_t i = 0; i < vOut->numFields(); i++) {
				auto fOut = vOut->field(i);
				if (fOut.type.id == vkm::vk::reflect::type::vkBool32) {
					*static_cast<VkBool32*>(fOut.ptr) = VK_FALSE;
				}
			}
		} else {
			for (size_t i = 0; i < vOut->numFields(); i++) {
				auto fIn = vIn->field(i);
				auto fOut = vOut->field(i);
				if (fOut.type.id == vkm::vk::reflect::type::vkBool32) {
					*static_cast<VkBool32*>(fOut.ptr) = *static_cast<VkBool32*>(fIn.ptr);
				}
			}
		}
	}
};

class initializer {
   private:
	vkm_initializer_preferType preferType;
	uint32_t requiredAPI;

	vkm::std::vector<vkm::std::string<char>> haveInstanceExtensions;
	vkm::std::vector<vkm::std::string<char>> requiredInstanceExtensions;
	vkm::std::vector<vkm::std::string<char>> optionalInstanceExtensions;
	vkm::std::vector<vkm::std::string<char>> enabledInstanceExtensions;

	vkm_initializer_vetoFn veto;

	struct rejectReason {
		VkPhysicalDevice physicalDevice;
		vkm::std::stringbuilder<char> reason;
	};
	vkm::std::vector<rejectReason> rejected;

	vkm::std::vector<vkm::std::string<char>> requiredDeviceExtensions;
	vkm::std::vector<vkm::std::string<char>> optionalDeviceExtensions;
	vkm::std::vector<vkm::std::string<char>> enabledDeviceExtensions;

	featureChain requiredFeatureChain;
	featureChain optionalFeatureChain;
	featureChain enabledFeatureChain;

	vkm::std::vector<vkm::std::pair<VkFormat, VkFormatFeatureFlags2>> requiredFormatFeatures;

	vkm::std::vector<VkSurfaceKHR> targetSurfaces;

	struct queueRequirements {
		uint32_t min;
		uint32_t max;
		struct {
			vkm::std::vector<vkm::std::smartPtr<vkm::vk::reflect::vkStructureChain>> pNext;
			VkDeviceQueueCreateFlags flags;
			uint32_t count;
			uint32_t family;
			vkm::std::vector<float> priorities;
		} createInfo;
	};
	queueRequirements graphicsQueueRequirements = {};
	queueRequirements computeQueueRequirements = {};
	queueRequirements transferQueueRequirements = {};

	vkm::std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	template <typename... Args>
	void appendRejectReason(const char* fmt, Args... args) noexcept {
		if (this->rejected.last().reason.size() > 0) {
			this->rejected.last().reason.write("\n");
		}
		this->rejected.last().reason.write(fmt, args...);
	}

	[[nodiscard]] bool scanInstanceExtensions() noexcept;

	[[nodiscard]] bool checkFeaturesConfig() noexcept;
	[[nodiscard]] bool checkQueueConfig() noexcept;
	[[nodiscard]] bool checkConfig() noexcept;

	[[nodiscard]] auto getDevices() const noexcept;

	[[nodiscard]] bool findProperties(VkPhysicalDevice) noexcept;
	[[nodiscard]] bool findExtensions(VkPhysicalDevice) noexcept;
	[[nodiscard]] bool findFeatures(VkPhysicalDevice) noexcept;
	[[nodiscard]] bool findFormats(VkPhysicalDevice) noexcept;
	[[nodiscard]] bool findQueues(VkPhysicalDevice) noexcept;
	[[nodiscard]] bool checkDevice(VkPhysicalDevice) noexcept;

	void checkOptionals(vkm_deviceInitInfo&) noexcept;

	// NOLINTBEGIN(readability-identifier-naming)
	friend VKM_FN void ::vkm_initializer_findExtension(vkm_initializer, VkBool32, vkm_string);
	friend VKM_FN void ::vkm_initializer_findFeature(vkm_initializer, VkBool32, void*);
	friend VKM_FN void ::vkm_initializer_findImageFormatFeature2(vkm_initializer, VkFormat, VkFormatFeatureFlags2);
	friend VKM_FN void ::vkm_initializer_findPresentationSupport(vkm_initializer, VkSurfaceKHR);
	friend VKM_FN void ::vkm_initializer_findGraphicsQueue(vkm_initializer, vkm_initializer_queueCreateInfo);
	friend VKM_FN void ::vkm_initializer_findComputeQueue(vkm_initializer, vkm_initializer_queueCreateInfo);
	friend VKM_FN void ::vkm_initializer_findTransferQueue(vkm_initializer, vkm_initializer_queueCreateInfo);

	friend VKM_FN auto ::vkm_initializer_getInstanceExtensionList(vkm_initializer, size_t*, const char**) -> VkResult;
	friend VKM_FN auto ::vkm_initializer_createInstance(vkm_initializer, VkInstance*) -> VkResult;
	friend VKM_FN auto ::vkm_initializer_createDevice(vkm_initializer, vkm_device*) -> VkResult;

	friend VKM_FN void ::vkm_initializer_getEnabledExtensions(vkm_initializer, size_t*, const char**);
	friend VKM_FN void ::vkm_initializer_getEnabledFeatures(vkm_initializer, void*);
	friend VKM_FN void ::vkm_initializer_getGraphicsQueueInfo(vkm_initializer, vkm_initializer_queueInfo*);
	friend VKM_FN void ::vkm_initializer_getComputeQueueInfo(vkm_initializer, vkm_initializer_queueInfo*);
	friend VKM_FN void ::vkm_initializer_getTransferQueueInfo(vkm_initializer, vkm_initializer_queueInfo*);
	friend VKM_FN void ::vkm_initializer_getRejectReasons(vkm_initializer, size_t*, vkm_initializer_rejectReason*);
	// NOLINTEND(readability-identifier-naming)

   public:
	initializer(vkm_initializerCreateInfo info) noexcept
		: preferType(info.preferType), requiredAPI(info.api), veto(info.vetoFn) {}

	[[nodiscard]] vkm_initializer handle() noexcept { return reinterpret_cast<vkm_initializer>(this); }
	[[nodiscard]] static initializer* fromHandle(vkm_initializer handle) noexcept {
		return reinterpret_cast<initializer*>(handle);
	}
};
}  // namespace vkm::vk::initializer
