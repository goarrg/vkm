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

// vulkan's VK_MAKE_API_VERSION includes type in the macro, this does not allow
// us to #if X < Y
#define VKM_MAKE_VK_API_VERSION(variant, major, minor, patch) \
	(((variant) << 29U) | ((major) << 22U) | ((minor) << 12U) | (patch))

#define VKM_VK_API_VERSION_1_3 VKM_MAKE_VK_API_VERSION(0, 1, 3, 0)
#define VKM_VK_API_VERSION_1_4 VKM_MAKE_VK_API_VERSION(0, 1, 4, 0)

// lowest api vkm was designed to work with
#define VKM_VK_MIN_API VKM_VK_API_VERSION_1_3
// highest known api to work with vkm
#define VKM_VK_MAX_API VKM_VK_API_VERSION_1_4

#if VKM_VK_MIN_API > VKM_VK_MAX_API
#error Min API cannot be > Max API
#endif
