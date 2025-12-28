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

#include <stddef.h>
#include <stdint.h>

// prevents including vulkan.h and therefore windows.h
// this block must be before anything else that includes vulkan
#define VULKAN_H_  // NOLINT(readability-identifier-naming)
#define VK_NO_PROTOTYPES
// #define VK_USE_64_BIT_PTR_DEFINES 0
#include <vulkan/vulkan_core.h>	 // IWYU pragma: export
#include "vkm_config.h"			 // IWYU pragma: export

#include "vkm/std/string.hpp"

#ifndef VKM_VK_API
// this is to support eventually letting users select which api to compile for,
// reducing code gen bloat
#define VKM_VK_API VKM_VK_MIN_API
#endif

#define VKM_FN __attribute__((nothrow))
#define VKM_HANDLE(object) typedef struct object##_T* object

#ifdef __cplusplus
extern "C" {
#endif

VKM_HANDLE(vkm_initializer);
VKM_HANDLE(vkm_device);
VKM_HANDLE(vkm_allocation);
VKM_HANDLE(vkm_swapchain);
VKM_HANDLE(vkm_context);

typedef enum {
	VKM_LOG_LEVEL_VERBOSE,
	VKM_LOG_LEVEL_INFO,
	VKM_LOG_LEVEL_WARN,
	VKM_LOG_LEVEL_ERROR,
	VKM_LOG_LEVEL_MAX_ENUM = 0x7FFFFFFF,
} vkm_logLevel;

typedef void (*vkm_loggerFn)(vkm_logLevel, size_t numTags, const vkm_string* tags, vkm_string message);

typedef struct {
	vkm_loggerFn loggerFn;
	// must not be null, instance creation depends on it
	PFN_vkGetInstanceProcAddr procAddr;
	// if null, indicates instance will be created later through vkm_deviceSelector,
	// will return VK_INCOMPLETE
	VkInstance vkInstance;
	// if true, destroys vkInstance at vkm_shutdown
	// this is always true for instances created from selectors
	VkBool32 gainOwnership;
} vkm_initInfo;

// this is not a UUID returned by any vulkan function call and is only valid within vkm
typedef uint8_t vkm_device_uuid[VK_UUID_SIZE];

typedef enum {
	// searches according to system device order
	VKM_INITIALIZER_PREFER_SYSTEM = 0,
	// searches intergrated gpus first
	VKM_INITIALIZER_PREFER_INTEGRATED = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
	// searches discrete gpus first
	VKM_INITIALIZER_PREFER_DISCRETE = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
	VKM_INITIALIZER_PREFER_MAX_ENUM = 0x7FFFFFFF,
} vkm_initializer_preferType;

typedef VkBool32 (*vkm_initializer_vetoFn)(VkPhysicalDevice, vkm_device_uuid);

typedef struct {
	uint32_t api;
	vkm_initializer_preferType preferType;
	vkm_initializer_vetoFn vetoFn;
} vkm_initializerCreateInfo;

typedef struct {
	const void* pNext;
	VkDeviceQueueCreateFlags flags;
	uint32_t min;
	uint32_t max;
	// priorities if not null must be of max length, defaults to 1.0
	const float* pPriorities;
} vkm_initializer_queueCreateInfo;

typedef struct {
	VkPhysicalDevice vkPhysicalDevice;
	const char* reason;
} vkm_initializer_rejectReason;

typedef struct {
	uint32_t family;
	uint32_t count;
} vkm_initializer_queueInfo;

typedef struct {
	VkPhysicalDevice vkPhysicalDevice;
	VkDevice vkDevice;
	// if true, destroys vkDevice at vkm_device_destroy
	// this is always true for devices created from initializers
	VkBool32 gainOwnership;
	struct {
		VkBool32 extSwapchainMaint1;
	} optionalFeatures;
} vkm_deviceInitInfo;

typedef struct {
	float minPointSize;
	float maxPointSize;

	float minLineWidth;
	float maxLineWidth;

	struct {
		uint64_t maxAllocationSize;
		uint32_t maxMemoryAllocationCount;
		uint32_t maxSamplerAllocationCount;
	} global;

	struct {
		uint32_t maxImageDimension1D;
		uint32_t maxImageDimension2D;
		uint32_t maxImageDimension3D;
		uint32_t maxImageDimensionCube;
		uint32_t maxImageArrayLayers;
		float maxSamplerAnisotropy;
		uint32_t maxUBOSize;
		uint32_t maxSBOSize;
	} perDescriptor;

	struct {
		uint32_t maxSamplerCount;
		uint32_t maxSampledImageCount;
		uint32_t maxCombinedImageSamplerCount;
		uint32_t maxStorageImageCount;

		uint32_t maxUBOCount;
		uint32_t maxSBOCount;
		uint32_t maxResourceCount;
	} perStage;

	struct {
		uint32_t maxSamplerCount;
		uint32_t maxSampledImageCount;
		uint32_t maxCombinedImageSamplerCount;
		uint32_t maxStorageImageCount;

		uint32_t maxUBOCount;
		uint32_t maxSBOCount;

		uint32_t maxBoundDescriptorSets;
		uint32_t maxPushConstantsSize;
	} perPipeline;

	struct {
		VkExtent3D maxDispatchSize;
		VkExtent3D maxWorkgroupSize;
		uint32_t minSubgroupSize;
		uint32_t maxSubgroupSize;
		struct {
			uint32_t maxInvocations;
			uint32_t maxSubgroupCount;
		} workgroup;
	} compute;
} vkm_device_limits;

typedef struct {
	vkm_device_uuid uuid;
	uint32_t vendorID;
	uint32_t deviceID;
	uint32_t driverVersion;
	uint32_t api;

	struct {
		uint32_t subgroupSize;
	} compute;

	vkm_device_limits limits;
} vkm_device_properties;

typedef enum {
#define VKM_VKFN(FN) VKM_VKFN_##FN,
#include "vkm/inc/vkfn_dispatch_instance.inc"
#include "vkm/inc/vkfn_dispatch_device.inc"
#undef VKM_VKFN

	VKM_VKFN_COUNT,
	VKM_VKFN_MAX_ENUM = 0x7FFFFFFF,
} vkm_vkfn_id;

typedef enum {
#define VKM_VKFN(FN) VKM_DEVICE_VKFN_##FN,
#include "vkm/inc/vkfn_dispatch_device.inc"
#undef VKM_VKFN

	VKM_DEVICE_VKFN_COUNT,
	VKM_DEVICE_VKFN_MAX_ENUM = 0x7FFFFFFF,
} vkm_device_vkfn_id;

typedef struct {
#define VKM_VKFN(fn) PFN_##fn fn;
#include "vkm/inc/vkfn_dispatch_instance.inc"
#include "vkm/inc/vkfn_dispatch_device.inc"
#undef VKM_VKFN
} vkm_device_dispatchTable;

typedef struct {
	vkm_allocation allocation;
	VkBuffer vkBuffer;
	void* ptr;
} vkm_hostBuffer;

typedef struct {
	vkm_allocation allocation;
	VkBuffer vkBuffer;
} vkm_deviceBuffer;

typedef struct {
	vkm_allocation allocation;
	VkImage vkImage;
} vkm_image;

typedef struct {
	VkSurfaceKHR targetSurface;
	VkExtent2D extent;
	// defaults to VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT if 0
	VkImageUsageFlags requiredUsage;

	// you may get more, you will get less if count exceeds limits
	// typical value is (numFramesInFlight + 1)
	uint32_t preferredImageCount;

	// if 0, defaults to SRGB
	// returns VK_ERROR_FORMAT_NOT_SUPPORTED if none found
	size_t numPreferredVkSurfaceFormats;
	VkSurfaceFormatKHR* pPreferredVkSurfaceFormats;

	// if 0, defaults to {FIFO_RELAXED, FIFO}
	// you will get FIFO if none found
	size_t numPreferredVkPresentModes;
	VkPresentModeKHR* pPreferredVkPresentModes;
} vkm_swapchainCreateInfo;

typedef struct {
	VkSurfaceFormatKHR vkSurfaceFormat;
	VkExtent2D extent;
	uint32_t numImages;
} vkm_swapchain_properties;

typedef struct {
	uint32_t index;
	VkImage vkImage;
	VkImageView vkImageView;
} vkm_swapcain_image;

typedef struct {
	vkm_swapchain swapchain;
	// if == 0, defaults to ALL_COMMANDS
	VkPipelineStageFlags2 stage;
	// pointer to where to store the acquired image
	vkm_swapcain_image* pImage;
	// pointer to where to store the vkResult of the acquire
	VkResult* pResult;
} vkm_swapchain_acquireInfo;

typedef struct {
	vkm_swapchain swapchain;
	// if == 0, defaults to ALL_COMMANDS
	VkPipelineStageFlags2 stage;
	// pointer to where to store the vkResult of the present
	VkResult* pResult;
} vkm_swapchain_presentInfo;

typedef struct {
	uint32_t queueFamily;
	uint32_t queueIndex;
	// a frame is defined as one context begin/end pair
	uint32_t maxPendingFrames;
	struct {
		const void* pNext;
		VkCommandPoolCreateFlags flags;
	} commandPoolCreateInfo;
} vkm_contextCreateInfo;

typedef void (*vkm_destroyFn)(void*);

typedef struct {
	vkm_destroyFn fn;
	// data to be passed into fn, may be null
	void* data;
} vkm_destroyer;

typedef struct {
	const void* pNext;
	VkCommandBufferUsageFlags flags;
} vkm_context_commandBufferBeginInfo;

typedef struct {
	struct {
		const void* pNext;
	} commandBufferSubmitInfo;
	struct {
		const void* pNext;
		VkSubmitFlags flags;

		uint32_t numWaitSemaphores;
		VkSemaphoreSubmitInfo* pWaitSemaphores;

		uint32_t numSignalSemaphores;
		VkSemaphoreSubmitInfo* pSignalSemaphores;
	} submitInfo;
	uint32_t numPrsentInfos;
	vkm_swapchain_presentInfo* pPresentInfos;
} vkm_context_commandBufferEndInfo;

// this can be called before vkm_init
extern VKM_FN const char* const* vkm_getRequiredVkInstanceExtensions(size_t*);

// if vkInstance is null, returns VK_INCOMPLETE to indicate init has not yet finished
extern VKM_FN VkResult vkm_init(vkm_initInfo);
extern VKM_FN void vkm_shutdown(void);
#define VKM_VKFN(FN) ((PFN_##FN)vkm_getProcAddr(VKM_VKFN_##FN))
extern VKM_FN PFN_vkVoidFunction vkm_getProcAddr(vkm_vkfn_id);

// VK_ERROR_DEVICE_LOST represents the device matching UUID not being found
extern VKM_FN VkResult vkm_device_getVkPhysicalDeviceFromUUID(vkm_device_uuid, VkPhysicalDevice*);

extern VKM_FN void vkm_createInitializer(vkm_initializerCreateInfo, vkm_initializer*);
extern VKM_FN void vkm_destroyInitializer(vkm_initializer);
// accepts both instance and device extensions not promoted to core at VKM_VK_API
extern VKM_FN void vkm_initializer_findExtension(vkm_initializer, VkBool32 required, vkm_string);
// you can call this with a single struct or a whole struct chain,
// duplicated stypes will result in a logical ORing of features.
// extension feature structs promoted to core in VKM_VK_API are not allowed,
// provisional and os specific feature structs are also not allowed as we are not able to handle them atm.
extern VKM_FN void vkm_initializer_findFeature(vkm_initializer, VkBool32 required, void*);
extern VKM_FN void vkm_initializer_findImageFormatFeature2(vkm_initializer, VkFormat, VkFormatFeatureFlags2);
// since surfaces cannot exist before a vkInstance, this function can only be called after
// vkm_init with a valid vkInstance or after vkm_initializer_createInstance
// calling this also automatically requires VK_KHR_swapchain
extern VKM_FN void vkm_initializer_findPresentationSupport(vkm_initializer, VkSurfaceKHR);
extern VKM_FN void vkm_initializer_findGraphicsQueue(vkm_initializer, vkm_initializer_queueCreateInfo);
extern VKM_FN void vkm_initializer_findComputeQueue(vkm_initializer, vkm_initializer_queueCreateInfo);
extern VKM_FN void vkm_initializer_findTransferQueue(vkm_initializer, vkm_initializer_queueCreateInfo);

// this can be called without vkm_initializer_createInstance, for situations where you want to create the instance
// yourself, returns VK_ERROR_EXTENSION_NOT_PRESENT if not every required extension was found
extern VKM_FN VkResult vkm_initializer_getInstanceExtensionList(vkm_initializer, size_t*, const char**);
// create instance and finishes initialization, must be called after vkm_init with a null vkInstance
// VkInstance will be owned by the vkm runtime and will be destroyed at vkm_shutdown()
extern VKM_FN VkResult vkm_initializer_createInstance(vkm_initializer, VkInstance*);

// creates device and calls vkm_initDevice, must be called after vkm_init with a valid vkInstance
// or after vkm_initializer_createInstance
extern VKM_FN VkResult vkm_initializer_createDevice(vkm_initializer, vkm_device*);

// every initializer function from here will only have values after calling vkm_initializer_createDevice

// will contain both instance and device extensions
extern VKM_FN void vkm_initializer_getEnabledExtensions(vkm_initializer, size_t*, const char**);
// you can call this with a single struct or a whole struct chain
extern VKM_FN void vkm_initializer_getEnabledFeatures(vkm_initializer, void*);
extern VKM_FN void vkm_initializer_getGraphicsQueueInfo(vkm_initializer, vkm_initializer_queueInfo*);
extern VKM_FN void vkm_initializer_getComputeQueueInfo(vkm_initializer, vkm_initializer_queueInfo*);
extern VKM_FN void vkm_initializer_getTransferQueueInfo(vkm_initializer, vkm_initializer_queueInfo*);
// reject reasons only available for tried devices, devices that are sorted after the selected device will not appear,
extern VKM_FN void vkm_initializer_getRejectReasons(vkm_initializer, size_t*, vkm_initializer_rejectReason*);

extern VKM_FN VkResult vkm_initDevice(vkm_deviceInitInfo, vkm_device*);
extern VKM_FN void vkm_destroyDevice(vkm_device);
extern VKM_FN void vkm_device_getVkDevice(vkm_device, VkPhysicalDevice*, VkDevice*);
extern VKM_FN void vkm_device_getProperties(vkm_device, vkm_device_properties*);
#define VKM_DEVICE_VKFN(device, FN) ((PFN_##FN)vkm_device_getProcAddr(device, VKM_DEVICE_VKFN_##FN))
extern VKM_FN PFN_vkVoidFunction vkm_device_getProcAddr(vkm_device, vkm_device_vkfn_id);
extern VKM_FN void vkm_device_getDispatchTable(vkm_device, vkm_device_dispatchTable*);
extern VKM_FN VkResult vkm_device_waitIdle(vkm_device);

extern VKM_FN void vkm_createTimelineSemaphore(vkm_device, vkm_string, uint64_t initialValue, VkSemaphore*);
extern VKM_FN void vkm_destroyTimelineSemaphore(vkm_device, VkSemaphore);
extern VKM_FN void vkm_semaphore_timeline_signal(vkm_device, VkSemaphore, uint64_t);
extern VKM_FN void vkm_semaphore_timeline_wait(vkm_device, VkSemaphore, uint64_t);
extern VKM_FN uint64_t vkm_semaphore_timeline_getValue(vkm_device, VkSemaphore);

extern VKM_FN void vkm_createHostBuffer(vkm_device, vkm_string, VkBufferCreateInfo, vkm_hostBuffer*);
extern VKM_FN void vkm_destroyHostBuffer(vkm_device, vkm_hostBuffer);
extern VKM_FN void vkm_hostBuffer_write(vkm_device, vkm_hostBuffer, size_t off, size_t sz, void*);
extern VKM_FN void vkm_hostBuffer_read(vkm_device, vkm_hostBuffer, size_t off, size_t sz, void*);

extern VKM_FN void vkm_createDeviceBuffer(vkm_device, vkm_string, VkBufferCreateInfo, vkm_deviceBuffer*);
extern VKM_FN void vkm_destroyDeviceBuffer(vkm_device, vkm_deviceBuffer);

extern VKM_FN void vkm_getFormatProperties3(vkm_device, VkFormat, VkFormatProperties3*);
extern VKM_FN VkResult vkm_getImageFormatProperties2(vkm_device, VkPhysicalDeviceImageFormatInfo2, VkImageFormatProperties2*);
extern VKM_FN VkBool32 vkm_getFormatHasImageUsageFlags(vkm_device, VkFormat, VkImageUsageFlags);
extern VKM_FN void vkm_createImage(vkm_device, vkm_string, VkImageCreateInfo, vkm_image*);
extern VKM_FN void vkm_destroyImage(vkm_device, vkm_image);

extern VKM_FN void vkm_createImageView(vkm_device, vkm_string, VkImageViewCreateInfo, VkImageView*);
extern VKM_FN void vkm_destroyImageView(vkm_device, VkImageView);

extern VKM_FN void vkm_createSampler(vkm_device, vkm_string, VkSamplerCreateInfo, VkSampler*);
extern VKM_FN void vkm_destroySampler(vkm_device, VkSampler);

extern VKM_FN VkResult vkm_createSwapchain(vkm_device, vkm_string, vkm_swapchainCreateInfo, vkm_swapchain*);
extern VKM_FN void vkm_destroySwapchain(vkm_swapchain);
extern VKM_FN void vkm_swapchain_getProperties(vkm_swapchain, vkm_swapchain_properties*);
extern VKM_FN VkResult vkm_swapchain_resize(vkm_swapchain, VkExtent2D);
extern VKM_FN VkResult vkm_swapchain_changeVkPresentMode(vkm_swapchain, size_t, VkPresentModeKHR*, VkExtent2D);

extern VKM_FN void vkm_createContext(vkm_device, vkm_string, vkm_contextCreateInfo, vkm_context*);
extern VKM_FN void vkm_destroyContext(vkm_context);

extern VKM_FN VkBool32 vkm_context_getSwapchainPresentationSupport(vkm_context, vkm_swapchain);

extern VKM_FN void vkm_context_begin(vkm_context, vkm_string);
// marks object for destruction when this frame is done and waited on
extern VKM_FN void vkm_context_queueDestroyer(vkm_context, vkm_destroyer);
// returns a host buffer that is only valid for the duration of the frame
extern VKM_FN void vkm_context_createScratchHostBuffer(vkm_context, vkm_string, VkBufferCreateInfo, vkm_hostBuffer*);
// the next call to vkm_context_endCommandBuffer will wait on the acquire semaphore,
// you cannot acquire from the same swapchain again until calling vkm_context_endCommandBuffer with this swapchain
// passed in presentInfo, where it will signal the present semaphore, all other sync is the responsibility of the caller
extern VKM_FN void vkm_context_acquireSwapchain(vkm_context, size_t, vkm_swapchain_acquireInfo*);
// you can only have one command buffer active per context at any given time
extern VKM_FN void vkm_context_beginCommandBuffer(vkm_context, vkm_string, vkm_context_commandBufferBeginInfo, VkCommandBuffer*);
extern VKM_FN void vkm_context_endCommandBuffer(vkm_context, vkm_context_commandBufferEndInfo);
extern VKM_FN void vkm_context_end(vkm_context);
extern VKM_FN void vkm_context_wait(vkm_context);

#ifdef __cplusplus
}
#endif
