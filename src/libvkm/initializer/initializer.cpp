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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <new>	// IWYU pragma: keep

#include "vkm/std/algorithm.hpp"
#include "vkm/std/array.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/string.hpp"
#include "vkm/std/utility.hpp"
#include "vkm/std/stdlib.hpp"
#include "vkm/std/unit.hpp"

#include "vkm/vkm.h"  // IWYU pragma: associated
#include "vkm.hpp"
#include "vklog.hpp"
#include "reflect_struct.hpp"
#include "reflect_const.hpp"
#include "reflect_extension.hpp"
#include "device/device.hpp"
#include "initializer/initializer.hpp"

inline static VkDeviceSize vramSize(VkPhysicalDevice device) {
	VkPhysicalDeviceMemoryProperties memProperties = {};
	VK_PROC(vkGetPhysicalDeviceMemoryProperties)(device, &memProperties);

	VkDeviceSize memSize = 0;

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		auto type = memProperties.memoryTypes[i];
		auto heap = memProperties.memoryHeaps[type.heapIndex];
		if (vkm::std::cmpBitFlags(type.propertyFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
			memSize = vkm::std::max(heap.size, memSize);
		}
	}

	return memSize;
}

namespace vkm::vk::initializer {
[[nodiscard]] bool initializer::scanInstanceExtensions() noexcept {
	vkm::vPrintf("Finding instance extensions");
	this->enabledInstanceExtensions.resize(0);

	if (this->haveInstanceExtensions.size() == 0) {
		auto vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
			VKM_VKFN(vkGetInstanceProcAddr)(nullptr, "vkEnumerateInstanceExtensionProperties"));
		uint32_t sz;
		VkResult ret = vkEnumerateInstanceExtensionProperties(nullptr, &sz, nullptr);
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to get instance extension list: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
		auto list = vkm::std::vector<VkExtensionProperties>(sz);
		ret = vkEnumerateInstanceExtensionProperties(nullptr, &sz, list.get());
		if (ret != VK_SUCCESS) {
			vkm::fatal(vkm::std::sourceLocation::current(), "Failed to get instance extension list: %s",
					   vkm::vk::reflect::toString(ret).cStr());
		}
		for (auto& p : list) {
			this->haveInstanceExtensions.pushBack(p.extensionName);
		}
	}

	bool ok = true;
	vkm::std::stringbuilder builder;

	for (auto& e : this->requiredInstanceExtensions) {
		if (!this->haveInstanceExtensions.contains(e)) {
			ok = false;
			builder << "Failed to find required extension: " << e << "\n";
		} else {
			this->enabledInstanceExtensions.pushBack(e);
		}
	}
	for (auto& e : this->optionalInstanceExtensions) {
		if (this->haveInstanceExtensions.contains(e)) {
			this->enabledInstanceExtensions.pushBack(e);
		}
	}
	enabledInstanceExtensions.sortComptact();

	if (!ok) {
		vkm::ePrintf("%sfindInstanceExtensions: Fail", builder.cStr());
	} else {
		vkm::vPrintf("findInstanceExtensions: Pass");
	}

	return ok;
}
[[nodiscard]] bool initializer::checkConfig() noexcept {
	static constexpr vkm::std::array configChecks = {
		vkm::std::pair("checkFeatures", &initializer::checkFeaturesConfig),
		vkm::std::pair("checkQueueCreateInfo", &initializer::checkQueueConfig),
	};
	bool checksOK = true;
	for (auto check : configChecks) {
		if (!(this->*(check.second))()) {
			vkm::ePrintf("%s: Fail", check.first);
			checksOK = false;
		}
	}
	return checksOK;
}
// TODO: long term fix?
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] auto initializer::getDevices() const noexcept {
	uint32_t numDevices = 0;
	VkResult ret = VK_PROC(vkEnumeratePhysicalDevices)(vkm::vkInstance(), &numDevices, nullptr);
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed to get list of GPU devices: %s",
				   vkm::vk::reflect::toString(ret).cStr());
	}
	if (numDevices == 0) {
		vkm::fatal("Failed to get list of GPU devices: List is empty");
	}
	vkm::std::vector<VkPhysicalDevice> devices(numDevices);
	ret = VK_PROC(vkEnumeratePhysicalDevices)(vkm::vkInstance(), &numDevices, devices.get());
	if (ret != VK_SUCCESS) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Failed to get list of GPU devices: %s",
				   vkm::vk::reflect::toString(ret).cStr());
	}
	vkm::std::vector<vkm::std::pair<VkPhysicalDevice, vkm::std::array<uint8_t, VK_UUID_SIZE>>> list(numDevices);
	// no sane person is going to overflow uint16_t
	for (VKM_UUID_INDEX_TYPE i = 0; i < VKM_UUID_INDEX_TYPE(numDevices); i++) {
		list[i] = vkm::std::pair(devices[i], vkm::vk::device::generateUUID(devices[i], i));
	}
	if ((numDevices > 1) && (this->preferType != VKM_INITIALIZER_PREFER_SYSTEM)) {
		switch (this->preferType) {
			case VKM_INITIALIZER_PREFER_SYSTEM:
				vkm::fatal("Device sort order: System, should not be here");
				break;
			case VKM_INITIALIZER_PREFER_INTEGRATED:
				vkm::iPrintf("Device sort oder: Intergrated");
				break;
			case VKM_INITIALIZER_PREFER_DISCRETE:
				vkm::iPrintf("Device sort oder: Discrete");
				break;
			case VKM_INITIALIZER_PREFER_MAX_ENUM:
				vkm::fatal("Invalid vkm_initializer_preferType");
				break;
		}
		auto preferType = static_cast<VkPhysicalDeviceType>(this->preferType);
		::std::ranges::stable_sort(list.begin(), list.end(), [preferType](const auto& deviceA, const auto& deviceB) {
			VkPhysicalDeviceProperties propertiesA = {};
			VK_PROC(vkGetPhysicalDeviceProperties)(deviceA.first, &propertiesA);

			VkPhysicalDeviceProperties propertiesB = {};
			VK_PROC(vkGetPhysicalDeviceProperties)(deviceB.first, &propertiesB);

			if (propertiesA.deviceType == preferType) {
				if (propertiesB.deviceType != preferType) {
					return true;
				}
			} else if (propertiesB.deviceType == preferType) {
				return false;
			}
			if (propertiesA.apiVersion > propertiesB.apiVersion) {
				return true;
			}
			return vramSize(deviceA.first) > vramSize(deviceB.first);
		});
	}
	{
		vkm::std::stringbuilder builder;
		builder << "Detected Devices:";
		for (size_t i = 0; const auto& device : list) {
			VkPhysicalDeviceDriverProperties driverProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
			VkPhysicalDeviceProperties2 properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driverProperties};
			VK_PROC(vkGetPhysicalDeviceProperties2)(device.first, &properties);

			builder << "\n[" << i++ << "] ";

			switch (properties.properties.deviceType) {
				case VK_PHYSICAL_DEVICE_TYPE_OTHER:
					builder << "(Other) ";
					break;
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
					builder << "(Integrated) ";
					break;
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
					builder << "(Discrete) ";
					break;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
					builder << "(Virtual) ";
					break;
				case VK_PHYSICAL_DEVICE_TYPE_CPU:
					builder << "(Software) ";
					break;
				default:
					builder << "(UNKNOWN: " << properties.properties.deviceType << ") ";
					break;
			}

			builder << static_cast<const char*>(properties.properties.deviceName);

			builder << " UUID: ";
			static_assert(VK_UUID_SIZE == 16);
			for (size_t i = 0; i < 4; i++) {
				builder.write("%02X", device.second[i]);
			}
			builder << "-";
			for (size_t i = 4; i < 6; i++) {
				builder.write("%02X", device.second[i]);
			}
			builder << "-";
			for (size_t i = 6; i < 8; i++) {
				builder.write("%02X", device.second[i]);
			}
			builder << "-";
			for (size_t i = 8; i < 10; i++) {
				builder.write("%02X", device.second[i]);
			}
			builder << "-";
			for (size_t i = 10; i < 16; i++) {
				builder.write("%02X", device.second[i]);
			}

			builder.write(" VRAM: %.2f GiB",
						  static_cast<double>(vramSize(device.first)) / static_cast<double>(vkm::std::unit::memory::gibibyte));
			builder << " VK: " << VK_VERSION_MAJOR(properties.properties.apiVersion) << "."
					<< VK_VERSION_MINOR(properties.properties.apiVersion) << "."
					<< VK_VERSION_PATCH(properties.properties.apiVersion)
					<< " Driver: " << static_cast<const char*>(driverProperties.driverName) << " "
					<< static_cast<const char*>(driverProperties.driverInfo);
		}
		vkm::iPrintf(builder.cStr());
	}
	return vkm::std::move(list);
}
[[nodiscard]] bool initializer::checkDevice(VkPhysicalDevice vkPhysicalDevice) noexcept {
	static constexpr vkm::std::array deviceChecks = {
		vkm::std::pair("findProperties", &initializer::findProperties),
		vkm::std::pair("findFeatures", &initializer::findFeatures),	 // features must be before extensions as it adds extensions
		vkm::std::pair("findExtensions", &initializer::findExtensions),
		vkm::std::pair("findFormats", &initializer::findFormats),
		vkm::std::pair("findQueues", &initializer::findQueues),
	};
	bool checksOK = true;
	for (auto check : deviceChecks) {
		if (!(this->*(check.second))(vkPhysicalDevice)) {
			vkm::vPrintf("%s: Fail", check.first);
			this->appendRejectReason("%s: Fail", check.first);
			checksOK = false;
		} else {
			vkm::vPrintf("%s: Pass", check.first);
		}
	}
	return checksOK;
}
void initializer::checkOptionals(vkm_deviceInitInfo& info) noexcept {
	{
		vkm::std::array extensionChecks = {
			vkm::std::triple("extSwapchainMaint1", &info.optionalFeatures.extSwapchainMaint1,
							 vkm::std::vector<const char*>(vkm::std::array{
								 VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME})),
		};
		for (auto& check : extensionChecks) {
			const size_t i = vkm::std::linearSearch(this->enabledDeviceExtensions.size(), [&](size_t i) -> bool {
				for (auto& want : check.third) {
					if (this->enabledDeviceExtensions[i] == want) {
						return true;
					}
				}
				return false;
			});
			if (i < this->enabledDeviceExtensions.size()) {
				*check.second = VK_TRUE;
				vkm::vPrintf("Optional feature %s: Enabled", check.first);
			}
		}
	}
}
}  // namespace vkm::vk::initializer

extern "C" {
VKM_FN void vkm_createInitializer(vkm_initializerCreateInfo info, vkm_initializer* initializerHandle) {
	auto* initializer = new (::std::nothrow) vkm::vk::initializer::initializer(info);
	*initializerHandle = initializer->handle();

	{
		size_t numExt;
		const auto* exts = vkm_getRequiredVkInstanceExtensions(&numExt);
		for (size_t i = 0; i < numExt; i++) {
			vkm_initializer_findExtension(initializer->handle(), VK_TRUE, VKM_MAKE_STRING(exts[i]));
		}
	}
	{
		auto features13 = VkPhysicalDeviceVulkan13Features{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.synchronization2 = VK_TRUE,
			.maintenance4 = VK_TRUE,
		};
		auto features12 = VkPhysicalDeviceVulkan12Features{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &features13,
			.timelineSemaphore = VK_TRUE,
			.bufferDeviceAddress = VK_TRUE,
		};
		auto features11 = VkPhysicalDeviceVulkan11Features{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			.pNext = &features12,
		};
		auto features10 = VkPhysicalDeviceFeatures2{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &features11,
		};
		vkm_initializer_findFeature(*initializerHandle, VK_TRUE, &features10);
	}
}
VKM_FN void vkm_destroyInitializer(vkm_initializer initializerHandle) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	delete initializer;
}
VKM_FN void vkm_initializer_findExtension(vkm_initializer initializerHandle, VkBool32 require, vkm_string extension) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	const auto* e = vkm::vk::reflect::extension(vkm::std::string(extension).cStr());
	if (e == nullptr) {
		vkm::fatal(vkm::std::sourceLocation::current(), "Cannot add unknown extension: %s", vkm::std::string(extension).cStr());
	}
	auto addExt = [require](auto& e, auto& requiredList, auto& optionalList) {
		if (require == VK_TRUE) {
			if (requiredList.contains(e->name)) {
				return;
			}
			requiredList.pushBack(vkm::std::string<char>(e->name));
		} else {
			if (requiredList.contains(e->name)) {
				return;
			}
			if (optionalList.contains(e->name)) {
				return;
			}
			optionalList.pushBack(vkm::std::string<char>(e->name));
		}
	};
	if (e->kind == vkm::vk::reflect::extensionInfo::kind::instance) {
		addExt(e, initializer->requiredInstanceExtensions, initializer->optionalInstanceExtensions);
	} else if (e->kind == vkm::vk::reflect::extensionInfo::kind::device) {
		addExt(e, initializer->requiredDeviceExtensions, initializer->optionalDeviceExtensions);
	}
	{
		if (require == VK_TRUE) {
			for (size_t i = 0; i < e->numInstanceDependencies(); i++) {
				const auto* d = e->instanceDependency(i);
				initializer->requiredInstanceExtensions.pushBack(d);
			}
			initializer->requiredInstanceExtensions.sortComptact();
			for (size_t i = 0; i < e->numDeviceDependencies(); i++) {
				const auto* d = e->deviceDependency(i);
				initializer->requiredDeviceExtensions.pushBack(d);
			}
			initializer->requiredDeviceExtensions.sortComptact();
		} else {
			for (size_t i = 0; i < e->numInstanceDependencies(); i++) {
				const auto* d = e->instanceDependency(i);
				initializer->optionalInstanceExtensions.pushBack(d);
			}
			initializer->optionalInstanceExtensions.sortComptact();
			for (size_t i = 0; i < e->numDeviceDependencies(); i++) {
				const auto* d = e->deviceDependency(i);
				initializer->optionalDeviceExtensions.pushBack(d);
			}
			initializer->optionalDeviceExtensions.sortComptact();
		}
	}
}
VKM_FN void vkm_initializer_findFeature(vkm_initializer initializerHandle, VkBool32 require, void* ptr) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	auto* features = reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(ptr);
	do {
		if (features->sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
			const auto* t = vkm::vk::reflect::device::featureStruct::typeOf(features->sType);
			if (t->numDependencies() == 1) {
				const auto* depend = t->dependency(0);
				static_assert(vkm::std::strlen("VK_VERSION_1_") == 13);
				if (strncmp(depend, "VK_VERSION_1_", 13) == 0) {
					// NOLINTNEXTLINE(bugprone-unchecked-string-to-number-conversion)
					const uint32_t minor = atoi(depend + 13);
					const uint32_t requiredVersion = VK_MAKE_API_VERSION(0, 1, minor, 0);
					if (requiredVersion > initializer->requiredAPI) {
						vkm::fatal(vkm::std::sourceLocation::current(),
								   "%s requires vulkan 1.%d but initializer was created for 1.%d", t->name, minor,
								   VK_API_VERSION_MINOR(initializer->requiredAPI));
					}
				} else {
					const auto* e = vkm::vk::reflect::extension(depend);
					vkm_initializer_findExtension(initializerHandle, require, VKM_MAKE_STRING(e->name));
				}
			}
		}
		if (require == VK_TRUE) {
			initializer->requiredFeatureChain.append(features);
			initializer->optionalFeatureChain.append(features->sType);
		} else {
			initializer->requiredFeatureChain.append(features->sType);
			initializer->optionalFeatureChain.append(features);
		}
	} while ((features = features->pNext) != nullptr);
}
VKM_FN void vkm_initializer_findImageFormatFeature2(vkm_initializer initializerHandle, VkFormat format, VkFormatFeatureFlags2 feature) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	initializer->requiredFormatFeatures.pushBack(vkm::std::pair(format, feature));
}
VKM_FN void vkm_initializer_findPresentationSupport(vkm_initializer initializerHandle, VkSurfaceKHR surface) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	initializer->targetSurfaces.pushBack(surface);
	vkm_initializer_findExtension(initializerHandle, VK_TRUE, VKM_MAKE_STRING(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
}
VKM_FN void vkm_initializer_findGraphicsQueue(vkm_initializer initializerHandle, vkm_initializer_queueCreateInfo info) {
	if (info.max == 0) {
		info.max = info.min;
	}
	if (info.max < info.min) {
		vkm::fatal("Cannot require graphics queue with max < min");
	}
	if (info.max == 0 && info.min == 0) {
		vkm::fatal("Cannot require graphics queue with max = min = 0");
	}
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	initializer->graphicsQueueRequirements.createInfo.pNext = vkm::std::move(
		vkm::vk::reflect::cloneVkStructureChain(reinterpret_cast<const vkm::vk::reflect::vkStructureChain*>(info.pNext)));
	initializer->graphicsQueueRequirements.createInfo.flags = info.flags;
	initializer->graphicsQueueRequirements.min = info.min;
	initializer->graphicsQueueRequirements.max = info.max;
	if (info.pPriorities != nullptr) {
		initializer->graphicsQueueRequirements.createInfo.priorities.resize(info.max);
		for (size_t i = 0; i < info.max; i++) {
			initializer->graphicsQueueRequirements.createInfo.priorities[i] = info.pPriorities[i];
		}
	} else {
		initializer->graphicsQueueRequirements.createInfo.priorities.resize(0);
		initializer->graphicsQueueRequirements.createInfo.priorities.resize(info.max, 1.0f);
	}
}
VKM_FN void vkm_initializer_findComputeQueue(vkm_initializer initializerHandle, vkm_initializer_queueCreateInfo info) {
	if (info.max == 0) {
		info.max = info.min;
	}
	if (info.max < info.min) {
		vkm::fatal("Cannot require compute queue with max < min");
	}
	if (info.max == 0 && info.min == 0) {
		vkm::fatal("Cannot require compute queue with max = min = 0");
	}
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	initializer->computeQueueRequirements.createInfo.pNext = vkm::std::move(
		vkm::vk::reflect::cloneVkStructureChain(reinterpret_cast<const vkm::vk::reflect::vkStructureChain*>(info.pNext)));
	initializer->computeQueueRequirements.createInfo.flags = info.flags;
	initializer->computeQueueRequirements.min = info.min;
	initializer->computeQueueRequirements.max = info.max;
	if (info.pPriorities != nullptr) {
		initializer->computeQueueRequirements.createInfo.priorities.resize(info.max);
		for (size_t i = 0; i < info.max; i++) {
			initializer->computeQueueRequirements.createInfo.priorities[i] = info.pPriorities[i];
		}
	} else {
		initializer->computeQueueRequirements.createInfo.priorities.resize(0);
		initializer->computeQueueRequirements.createInfo.priorities.resize(info.max, 1.0f);
	}
}
VKM_FN void vkm_initializer_findTransferQueue(vkm_initializer initializerHandle, vkm_initializer_queueCreateInfo info) {
	if (info.max == 0) {
		info.max = info.min;
	}
	if (info.max < info.min) {
		vkm::fatal("Cannot require transfer queue with max < min");
	}
	if (info.max == 0 && info.min == 0) {
		vkm::fatal("Cannot require transfer queue with max = min = 0");
	}
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	initializer->transferQueueRequirements.createInfo.pNext = vkm::std::move(
		vkm::vk::reflect::cloneVkStructureChain(reinterpret_cast<const vkm::vk::reflect::vkStructureChain*>(info.pNext)));
	initializer->transferQueueRequirements.createInfo.flags = info.flags;
	initializer->transferQueueRequirements.min = info.min;
	initializer->transferQueueRequirements.max = info.max;
	if (info.pPriorities != nullptr) {
		initializer->transferQueueRequirements.createInfo.priorities.resize(info.max);
		for (size_t i = 0; i < info.max; i++) {
			initializer->transferQueueRequirements.createInfo.priorities[i] = info.pPriorities[i];
		}
	} else {
		initializer->transferQueueRequirements.createInfo.priorities.resize(0);
		initializer->transferQueueRequirements.createInfo.priorities.resize(info.max, 1.0f);
	}
}

VKM_FN VkResult vkm_initializer_getInstanceExtensionList(vkm_initializer initializerHandle, size_t* sz, const char** out) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	if (!initializer->scanInstanceExtensions()) {
		*sz = 0;
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	if (out == nullptr) {
		*sz = initializer->enabledInstanceExtensions.size();
		return VK_SUCCESS;
	}
	*sz = vkm::std::min(*sz, initializer->enabledInstanceExtensions.size());
	size_t off = 0;
	for (size_t i = 0; i < initializer->enabledInstanceExtensions.size() && (off < *sz); i++) {
		out[off++] = initializer->enabledInstanceExtensions[i].cStr();
	}
	return VK_SUCCESS;
}
VKM_FN VkResult vkm_initializer_createInstance(vkm_initializer initializerHandle, VkInstance* vkInstance) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);

	if (!initializer->scanInstanceExtensions()) {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	vkm::iPrintf("Creating instance");
	{
		for (auto& e : initializer->enabledInstanceExtensions) {
			vkm::vPrintf("Enabled extension: %s", e.cStr());
		}
	}
	vkm::std::vector<const char*> extensions(initializer->enabledInstanceExtensions);
	const VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.apiVersion = initializer->requiredAPI,
	};
	// NOLINTNEXTLINE(misc-const-correctness)
	VkInstanceCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
		.ppEnabledExtensionNames = extensions.get(),
	};
#ifndef NDEBUG
	const VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,

#if VKM_LOG_LEVEL <= VKM_LOG_LEVEL_WARN
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
#else
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
#endif

		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
					   | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = vkm::vkLogger,
	};

	createInfo.pNext = &messengerCreateInfo;
#endif
	// we do not add vkGetInstanceProcAddr into the enum list as this should only ever be called once
	const VkResult ret = reinterpret_cast<PFN_vkCreateInstance>(
		VKM_VKFN(vkGetInstanceProcAddr)(nullptr, "vkCreateInstance"))(&createInfo, nullptr, vkInstance);
	if (ret != VK_SUCCESS) {
		return ret;
	}
	return vkm::initInstance(*vkInstance, true);
}
VKM_FN VkResult vkm_initializer_createDevice(vkm_initializer initializerHandle, vkm_device* instanceHandle) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);

	vkm::iPrintf("Finding device");
	if (!initializer->checkConfig()) {
		vkm::fatal("Failed initializer config checks");
	}

	auto devices = initializer->getDevices();
	initializer->rejected.resize(0);
	for (size_t i = 0; i < devices.size(); i++) {
		vkm::iPrintf("Checking device: [%d]", i);
		auto& device = devices[i];
		vkm_deviceInitInfo info = {
			.vkPhysicalDevice = device.first,
			.gainOwnership = VK_TRUE,
		};
		initializer->rejected.pushBack({device.first, {}});
		if ((initializer->veto != nullptr) && (initializer->veto(device.first, device.second.get()) == VK_TRUE)) {
			initializer->appendRejectReason("Vetoed");
			continue;
		}
		if (!initializer->checkDevice(info.vkPhysicalDevice)) {
			continue;
		}
		vkm::iPrintf("Selected device: [%d]", i);
		initializer->checkOptionals(info);
		{
			vkm::std::vector<const char*> extensions(initializer->enabledDeviceExtensions);
			const VkDeviceCreateInfo createInfo = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = &initializer->enabledFeatureChain.start,

				.queueCreateInfoCount = static_cast<uint32_t>(initializer->queueCreateInfos.size()),
				.pQueueCreateInfos = initializer->queueCreateInfos.get(),

				.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
				.ppEnabledExtensionNames = extensions.get(),
			};
			const VkResult ret = VK_PROC(vkCreateDevice)(device.first, &createInfo, nullptr, &info.vkDevice);
			if (ret != VK_SUCCESS) {
				vkm::iPrintf("Failed to initialize device: %s", vkm::vk::reflect::toString(ret).cStr());
				initializer->appendRejectReason("Failed to initialize device: %s", vkm::vk::reflect::toString(ret).cStr());
				continue;
			}
		}

		initializer->rejected.popBack();
		const VkResult ret = vkm_initDevice(info, instanceHandle);
		if (ret != VK_SUCCESS) {
			vkm::iPrintf("Failed to initialize device: %s", vkm::vk::reflect::toString(ret).cStr());
			return ret;
		}

		{
			for (auto& e : initializer->enabledDeviceExtensions) {
				vkm::vPrintf("Enabled extension: %s", e.cStr());
			}
		}

		vkm::std::debugRun([&]() {
			auto* instance = ::vkm::vk::device::instance::fromHandle(*instanceHandle);
			auto labelQueue = [&](const auto& requirements, const char* name) {
				for (uint32_t i = 0; i < requirements.createInfo.count; i++) {
					VkQueue vkQueue;
					VKM_DEVICE_VKFN(instance, vkGetDeviceQueue)(info.vkDevice, requirements.createInfo.family, i, &vkQueue);
					vkm::std::stringbuilder builder;
					builder << "queue_" << name << "_" << i;
					vkm::vk::debugLabel(info.vkDevice, vkQueue, builder.cStr());
				}
			};
			labelQueue(initializer->graphicsQueueRequirements, "graphics");
			labelQueue(initializer->computeQueueRequirements, "compute");
			labelQueue(initializer->transferQueueRequirements, "transfer");
		});
		return VK_SUCCESS;
	}

	vkm::ePrintf("No compatible devices found");
	return VK_ERROR_INITIALIZATION_FAILED;
}

VKM_FN void vkm_initializer_getEnabledExtensions(vkm_initializer initializerHandle, size_t* sz, const char** out) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);

	if (out == nullptr) {
		*sz = initializer->enabledInstanceExtensions.size() + initializer->enabledDeviceExtensions.size();
		return;
	}
	*sz = vkm::std::min(*sz, initializer->enabledInstanceExtensions.size() + initializer->enabledDeviceExtensions.size());
	size_t off = 0;
	for (size_t i = 0; i < initializer->enabledInstanceExtensions.size() && (off < *sz); i++) {
		out[off++] = initializer->enabledInstanceExtensions[i].cStr();
	}
	for (size_t i = 0; i < initializer->enabledDeviceExtensions.size() && (off < *sz); i++) {
		out[off++] = initializer->enabledDeviceExtensions[i].cStr();
	}
}
VKM_FN void vkm_initializer_getEnabledFeatures(vkm_initializer initializerHandle, void* ptr) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	auto* features = reinterpret_cast<vkm::vk::reflect::vkStructureChain*>(ptr);
	do {
		initializer->enabledFeatureChain.extract(features);
	} while ((features = features->pNext) != nullptr);
}
VKM_FN void vkm_initializer_getGraphicsQueueInfo(vkm_initializer initializerHandle, vkm_initializer_queueInfo* info) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	if (initializer->graphicsQueueRequirements.createInfo.count > 0) {
		*info = vkm_initializer_queueInfo{
			.family = initializer->graphicsQueueRequirements.createInfo.family,
			.count = initializer->graphicsQueueRequirements.createInfo.count,
		};
	} else {
		*info = {};
	}
}
VKM_FN void vkm_initializer_getComputeQueueInfo(vkm_initializer initializerHandle, vkm_initializer_queueInfo* info) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	if (initializer->computeQueueRequirements.createInfo.count > 0) {
		*info = vkm_initializer_queueInfo{
			.family = initializer->computeQueueRequirements.createInfo.family,
			.count = initializer->computeQueueRequirements.createInfo.count,
		};
	} else {
		*info = {};
	}
}
VKM_FN void vkm_initializer_getTransferQueueInfo(vkm_initializer initializerHandle, vkm_initializer_queueInfo* info) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	if (initializer->transferQueueRequirements.createInfo.count > 0) {
		*info = vkm_initializer_queueInfo{
			.family = initializer->transferQueueRequirements.createInfo.family,
			.count = initializer->transferQueueRequirements.createInfo.count,
		};
	} else {
		*info = {};
	}
}
VKM_FN void vkm_initializer_getRejectReasons(vkm_initializer initializerHandle, size_t* count, vkm_initializer_rejectReason* ptr) {
	auto* initializer = vkm::vk::initializer::initializer::fromHandle(initializerHandle);
	if (ptr == nullptr) {
		*count = initializer->rejected.size();
		return;
	}
	*count = vkm::std::min(*count, initializer->rejected.size());
	for (size_t i = 0; i < *count; i++) {
		ptr[i].vkPhysicalDevice = initializer->rejected[i].physicalDevice;
		ptr[i].reason = initializer->rejected[i].reason.cStr();
	}
}
}
