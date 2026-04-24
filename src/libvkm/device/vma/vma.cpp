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

#include "vkm.hpp"			   // IWYU pragma: keep
#include "vkm/std/array.hpp"   // IWYU pragma: keep
#include "vkm/std/string.hpp"  // IWYU pragma: keep

#ifndef NDEBUG
#define VMA_DEBUG_LOG(msg)                                            \
	do {                                                              \
		::vkm::vPrintf(vkm::std::array{VKM_MAKE_STRING("vma")}, msg); \
	} while (false)
#define VMA_DEBUG_LOG_FORMAT(format, ...)                                                           \
	do {                                                                                            \
		::vkm::vPrintf(vkm::std::array{VKM_MAKE_STRING("vma")}, format __VA_OPT__(, ) __VA_ARGS__); \
	} while (false)
#endif

#define VMA_IMPLEMENTATION
#include "device/vma/vma.hpp"  // IWYU pragma: keep
