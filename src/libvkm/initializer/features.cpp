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
#include <string.h>

#include "vkm/std/memory.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/string.hpp"
#include "vkm/std/stdlib.hpp"

#include "vkm/vkm.h"
#include "vkm.hpp"
#include "reflect_struct.hpp"
#include "initializer/initializer.hpp"

[[nodiscard]] bool vkm::vk::initializer::initializer::checkFeaturesConfig() noexcept {
	auto findExtension = [](auto& t, auto& extensionList) -> bool {
		for (size_t i = 0; i < t->numDependencies(); i++) {
			auto e = t->dependency(i);
			if (extensionList.contains(e)) {
				return true;
			}
		}
		return false;
	};
	auto checkChain = [findExtension](const char* name, auto* chain, auto& extensionList) -> bool {
		bool ok = true;
		for (; chain != nullptr; chain = chain->pNext) {
			auto v = vkm::vk::reflect::device::featureStruct::valueOf(chain);
			{
				bool hasValues = false;
				for (auto& f : *v.get()) {
					if (f.type.id == vkm::vk::reflect::type::vkBool32) {
						const VkBool32 set = *reinterpret_cast<VkBool32*>(f.ptr);
						if (set) {
							hasValues = true;
							break;
						}
					}
				}
				if (!hasValues) {
					continue;
				}
			}
			if (v->type->numDependencies() > 1) {
				if (!findExtension(v->type, extensionList)) {
					ok = false;
					vkm::std::stringbuilder builder;
					builder << name << " feature " << v->type->name
							<< " was passed but struct was associated with multiple extensions, "
							   "one of the following must be added to the "
							<< name << " extension list:";
					for (size_t i = 0; i < v->type->numDependencies(); i++) {
						builder << "\n" << v->type->dependency(i);
					}
					vkm::ePrintf(builder.cStr());
				}
			}
		}
		return ok;
	};

	if (!checkChain("Required", reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(requiredFeatureChain.start.pNext),
					requiredDeviceExtensions)) {
		return false;
	}
	if (!checkChain("Optional", reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(optionalFeatureChain.start.pNext),
					optionalDeviceExtensions)) {
		return false;
	}
	return true;
}
[[nodiscard]] bool vkm::vk::initializer::initializer::findFeatures(VkPhysicalDevice vkPhysicalDevice) noexcept {
	if (requiredFeatureChain.allocations.size() != optionalFeatureChain.allocations.size()) {
		vkm::fatal(vkm::std::sourceLocation::current(),
				   "Size mismatch between required and optional feature chains: %d != %d",
				   requiredFeatureChain.allocations.size(), optionalFeatureChain.allocations.size());
	}

	vkm::vk::initializer::featureChain haveFeatureChain;
	for (size_t i = 0; i < requiredFeatureChain.allocations.size(); i++) {
		if (requiredFeatureChain.allocations[i]->sType != optionalFeatureChain.allocations[i]->sType) {
			vkm::fatal(vkm::std::sourceLocation::current(),
					   "Required and optional feature chains must be in the same order: sType %d != %d",
					   requiredFeatureChain.allocations[i]->sType, optionalFeatureChain.allocations[i]->sType);
		}
		haveFeatureChain.append(requiredFeatureChain.allocations[i]->sType);
	}
	VK_PROC(vkGetPhysicalDeviceFeatures2)(vkPhysicalDevice, &haveFeatureChain.start);

	bool ok = true;

	auto* required = reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(&requiredFeatureChain.start);
	auto* optional = reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(&optionalFeatureChain.start);
	auto* enabled = reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(&enabledFeatureChain.start);
	auto* have = reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(&haveFeatureChain.start);

	auto rV = vkm::vk::reflect::device::featureStruct::valueOf(&requiredFeatureChain.start.features);
	auto oV = vkm::vk::reflect::device::featureStruct::valueOf(&optionalFeatureChain.start.features);
	auto eV = vkm::vk::reflect::device::featureStruct::valueOf(&enabledFeatureChain.start.features);
	auto hV = vkm::vk::reflect::device::featureStruct::valueOf(&haveFeatureChain.start.features);

	enabledFeatureChain.reset();
	auto enableFeature = [&](VkStructureType sType, size_t fieldIndex) {
		if (enabled == nullptr) {
			enabledFeatureChain.append(sType, 1, &fieldIndex);
			enabled = enabledFeatureChain.allocations.last().get();
			eV = vkm::vk::reflect::device::featureStruct::valueOf(enabled);
		} else {
			auto eF = eV->field(fieldIndex);
			auto* e = static_cast<VkBool32*>(eF.ptr);
			*e = VK_TRUE;
		}
	};

	while (have != nullptr) {
		for (size_t fieldIndex = 0; fieldIndex < hV->numFields(); fieldIndex++) {
			auto rF = rV->field(fieldIndex);
			auto oF = oV->field(fieldIndex);
			auto hF = hV->field(fieldIndex);
			switch (hF.type.id) {
				case vkm::vk::reflect::type::vkBool32: {
					auto r = *static_cast<VkBool32*>(rF.ptr);
					auto o = *static_cast<VkBool32*>(oF.ptr);
					auto h = *static_cast<VkBool32*>(hF.ptr);

					if (r == VK_TRUE) {
						if (h == VK_TRUE) {
							enableFeature(have->sType, fieldIndex);
						} else {
							this->appendRejectReason("Missing required feature %s.%s", hV->type->name, hF.name);
							ok = false;
						}
					} else if (o == VK_TRUE) {
						if (h == VK_TRUE) {
							enableFeature(have->sType, fieldIndex);
						}
					}
				} break;
				default:
					break;
			}
		}
		required = required->pNext;
		optional = optional->pNext;
		have = have->pNext;
		if (have != nullptr) {
			rV = vkm::vk::reflect::device::featureStruct::valueOf(required);
			oV = vkm::vk::reflect::device::featureStruct::valueOf(optional);
			hV = vkm::vk::reflect::device::featureStruct::valueOf(have);
		}
		if (enabled != nullptr) {
			enabled = enabled->pNext;
		}
	}

	return ok;
}
