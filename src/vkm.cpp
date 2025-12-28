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

#include <stdint.h>
#include <stddef.h>

#include "vkm/std/array.hpp"
#include "vkm/std/utility.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/string.hpp"

#include "vkm.hpp"
#include "vklog.hpp"  // IWYU pragma: keep
#include "reflect_const.hpp"

// Vulkan dependencies version check
static_assert(VK_HEADER_VERSION_COMPLETE >= VKM_VK_MAX_API);
static_assert(VKM_VK_MAX_API >= VKM_VK_MIN_API);
static_assert(VKM_VK_API >= VKM_VK_MIN_API);
static_assert(VKM_VK_API <= VKM_VK_MAX_API);
static_assert(VKM_VKFN_COUNT <= VKM_VKFN_MAX_ENUM);
static_assert(VKM_VKFN_COUNT == (sizeof(vkm_device_dispatchTable) / sizeof(PFN_vkVoidFunction)));

namespace {
VkInstance vkInstance = nullptr;
bool ownedInstance = false;
vkm::std::array<PFN_vkVoidFunction, VKM_VKFN_COUNT> vkfns;
vkm_loggerFn logger = nullptr;
#ifndef NDEBUG
VkDebugUtilsMessengerEXT vkMessenger = VK_NULL_HANDLE;
#endif

void nullLogger(vkm_logLevel, size_t, const vkm_string*, vkm_string) {}
}  // namespace

namespace vkm {
void log(vkm_logLevel level, const vkm::std::vector<vkm_string>&& tags, size_t n, const char* msg) noexcept {
	logger(level, tags.size(), tags.get(),
		   vkm_string{
			   .len = n,
			   .ptr = msg,
		   });
}
VkBool32 vkLogger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
				  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
	auto tags = vkm::std::vector<vkm::std::string<char>>();
	vkm::std::stringbuilder builder;

	if (vkm::std::cmpBitFlagsContains(messageTypes, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)) {
		tags.pushBack("VkGen");
	}
	if (vkm::std::cmpBitFlagsContains(messageTypes, VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)) {
		tags.pushBack("VkVal");
	}
	if (vkm::std::cmpBitFlagsContains(messageTypes, VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)) {
		tags.pushBack("VkPerf");
	}
	if (pCallbackData->pMessageIdName != nullptr) {
		builder << pCallbackData->pMessageIdName << ": " << pCallbackData->messageIdNumber;
		tags.pushBack(builder.str());
		builder.reset();
	} else {
		builder << "MessageId: " << pCallbackData->messageIdNumber;
		tags.pushBack(builder.str());
		builder.reset();
	}
	for (size_t i = 0; i < pCallbackData->queueLabelCount; i++) {
		builder << "VkQueue: " << pCallbackData->pQueueLabels[i].pLabelName;
		tags.pushBack(builder.str());
		builder.reset();
	}
	for (size_t i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
		builder << "VkCommandBuffer: " << pCallbackData->pCmdBufLabels[i].pLabelName;
		tags.pushBack(builder.str());
		builder.reset();
	}
	for (size_t i = 0; i < pCallbackData->objectCount; i++) {
		builder << "VkObj: " << vkm::vk::reflect::toString(pCallbackData->pObjects[i].objectType) << " ";
		if (pCallbackData->pObjects[i].pObjectName != nullptr) {
			builder << pCallbackData->pObjects[i].pObjectName << " ";
		}
		builder.write("0x%X", pCallbackData->pObjects[i].objectHandle);
		tags.pushBack(builder.str());
		builder.reset();
	}

	auto vkmTags = vkm::std::vector<vkm_string>(0, tags.size());
	for (auto& t : tags) {
		vkmTags.pushBack(t.vkm_string());
	}

	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		::logger(VKM_LOG_LEVEL_ERROR, vkmTags.size(), vkmTags.get(), VKM_MAKE_STRING(pCallbackData->pMessage));
	} else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		::logger(VKM_LOG_LEVEL_WARN, vkmTags.size(), vkmTags.get(), VKM_MAKE_STRING(pCallbackData->pMessage));
	} else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		::logger(VKM_LOG_LEVEL_INFO, vkmTags.size(), vkmTags.get(), VKM_MAKE_STRING(pCallbackData->pMessage));
	} else {
		::logger(VKM_LOG_LEVEL_VERBOSE, vkmTags.size(), vkmTags.get(), VKM_MAKE_STRING(pCallbackData->pMessage));
	}

	return VK_FALSE;
}
VkResult initInstance(VkInstance vkInstance, bool owned) {
	if (::vkInstance != nullptr) {
		vkm::fatal("Cannot init twice without calling vkm_shutdown");
	}
#ifndef NDEBUG
	if (::vkMessenger != VK_NULL_HANDLE) {
		vkm::fatal("vkMessenger is not null, this should never happen");
	}
#endif
	if (vkInstance == nullptr) {
		vkm::fatal("Cannot init null vkInstance");
	}
	::vkInstance = vkInstance;
	::ownedInstance = owned;
	{
		bool ok = true;
		vkm::std::stringbuilder<char> error;

#undef VK_PROC
#define VK_PROC(FN)                           \
	if (!VKM_VKFN(FN)) {                      \
		error.write("\nFailed to find " #FN); \
		ok = false;                           \
	}

#ifndef NDEBUG
#undef VK_DEBUG_PROC
#define VK_DEBUG_PROC(FN) VK_PROC(FN)
#endif

#include "vkfn.inc"	 // IWYU pragma: keep

#undef VK_PROC
#define VK_PROC(FN) VKM_VKFN(FN)
#ifndef NDEBUG
#undef VK_DEBUG_PROC
#define VK_DEBUG_PROC(FN) VKM_VKFN(FN)
#endif

		if (!ok) {
			vkm::ePrintf("[vkfn] Failed to find all required functions:%s", error.cStr());
			return VK_ERROR_INCOMPATIBLE_DRIVER;
		}
	}
	{
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

		const VkResult ret = VK_DEBUG_PROC(vkCreateDebugUtilsMessengerEXT)(
			vkm::vkInstance(), &messengerCreateInfo, nullptr, &::vkMessenger);
		if (ret != VK_SUCCESS) {
			vkm::ePrintf("vkCreateDebugUtilsMessengerEXT: %s", vkm::vk::reflect::toString(ret).cStr());
		}
#endif
	}
	return VK_SUCCESS;
}
VkInstance vkInstance() {
	return ::vkInstance;
}
}  // namespace vkm

extern "C" {
VKM_FN const char* const* vkm_getRequiredVkInstanceExtensions(size_t* count) {
#ifndef NDEBUG
	static constexpr vkm::std::array extensions = {
		&VK_EXT_DEBUG_UTILS_EXTENSION_NAME[0],
	};
	*count = extensions.size();
	return extensions.get();
#else
	*count = 0;
	return nullptr;
#endif
}
VKM_FN VkResult vkm_init(vkm_initInfo info) {
	if (info.procAddr == nullptr) {
		vkm::ePrintf("vkm_initInfo.procAddr must not be null");
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	if (info.loggerFn == nullptr) {
		info.loggerFn = nullLogger;
	}
	logger = info.loggerFn;
	vkfns.fill(nullptr);
	vkfns[VKM_VKFN_vkGetInstanceProcAddr] = reinterpret_cast<PFN_vkVoidFunction>(info.procAddr);

	if (info.vkInstance == nullptr) {
		ownedInstance = false;
		return VK_INCOMPLETE;
	}

	return vkm::initInstance(info.vkInstance, info.gainOwnership == VK_TRUE);
}
VKM_FN void vkm_shutdown() {
#ifndef NDEBUG
	VK_DEBUG_PROC(vkDestroyDebugUtilsMessengerEXT)(vkInstance, vkMessenger, nullptr);
	vkMessenger = VK_NULL_HANDLE;
#endif

	if (ownedInstance) {
		VK_PROC(vkDestroyInstance)(vkInstance, nullptr);
	}

	vkfns.fill(nullptr);
	logger = nullptr;
	vkInstance = nullptr;
}
// NOLINTNEXTLINE(readability-function-size)
VKM_FN PFN_vkVoidFunction vkm_getProcAddr(vkm_vkfn_id fn) {
	if (vkfns[fn] != nullptr) {
		return vkfns[fn];
	}

	switch (fn) {
#undef VKM_VKFN
#define VKM_VKFN(FN)                                                                                            \
	case VKM_VKFN_##FN:                                                                                         \
		vkfns[fn] = ((PFN_vkGetInstanceProcAddr)vkfns[VKM_VKFN_vkGetInstanceProcAddr])(vkm::vkInstance(), #FN); \
		return vkfns[fn];
#include "vkm/inc/vkfn_dispatch_instance.inc"  // IWYU pragma: keep
#include "vkm/inc/vkfn_dispatch_device.inc"	   // IWYU pragma: keep
#undef VKM_VKFN
#define VKM_VKFN(FN) ((PFN_##FN)vkm_getProcAddr(VKM_VKFN_##FN))

		case VKM_VKFN_COUNT:
		case VKM_VKFN_MAX_ENUM:
			return nullptr;
	}

	return nullptr;
}
}
