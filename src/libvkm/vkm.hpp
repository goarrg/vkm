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

#include <stdio.h>
#include <string.h>

#include "vkm/vkm.h"  // IWYU pragma: export

#include "vkm/std/stdlib.hpp"
#include "vkm/std/string.hpp"
#include "vkm/std/array.hpp"
#include "vkm/std/vector.hpp"

#ifndef VXR_LOG_LEVEL
#ifdef NDEBUG
#define VXR_LOG_LEVEL VXR_LOG_LEVEL_WARN
#else
#define VXR_LOG_LEVEL VXR_LOG_LEVEL_VERBOSE
#endif
#endif

namespace vkm {
void log(vkm_logLevel, const vkm::std::vector<vkm_string>&& tags, size_t, const char*) noexcept;
VkBool32 vkLogger(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
				  const VkDebugUtilsMessengerCallbackDataEXT*, void*);
VkResult initInstance(VkInstance, bool);
VkInstance vkInstance();

template <size_t N, typename... T>
inline static void vPrintf([[maybe_unused]] vkm::std::array<vkm_string, N>&& tags, [[maybe_unused]] const char* fmt,
						   [[maybe_unused]] T... args) noexcept {
#if VKM_LOG_LEVEL <= VKM_LOG_LEVEL_VERBOSE
	if constexpr (sizeof...(T) > 0) {
		//+1 for null terminator
		const int n = snprintf(nullptr, 0, fmt, args...) + 1;
		vkm::std::vector<char> buf(n);
		snprintf(buf.get(), n, fmt, args...);

		//-1 to get strlen
		log(VKM_LOG_LEVEL_VERBOSE, vkm::std::vector<vkm_string>(vkm::std::move(tags)), n - 1, buf.get());
	} else {
		log(VKM_LOG_LEVEL_VERBOSE, vkm::std::vector<vkm_string>(vkm::std::move(tags)), vkm::std::strlen(fmt),
			const_cast<char*>(fmt));
	}
#endif
}
template <typename... T>
inline static void vPrintf([[maybe_unused]] const char* fmt, [[maybe_unused]] T... args) noexcept {
#if VKM_LOG_LEVEL <= VKM_LOG_LEVEL_VERBOSE
	vPrintf<0, T...>({}, fmt, args...);
#endif
}
template <size_t N, typename... T>
inline static void iPrintf([[maybe_unused]] vkm::std::array<vkm_string, N>&& tags, [[maybe_unused]] const char* fmt,
						   [[maybe_unused]] T... args) noexcept {
#if VKM_LOG_LEVEL <= VKM_LOG_LEVEL_INFO
	if constexpr (sizeof...(T) > 0) {
		//+1 for null terminator
		const int n = snprintf(nullptr, 0, fmt, args...) + 1;
		vkm::std::vector<char> buf(n);
		snprintf(buf.get(), n, fmt, args...);

		//-1 to get strlen
		log(VKM_LOG_LEVEL_INFO, vkm::std::vector<vkm_string>(vkm::std::move(tags)), n - 1, buf.get());
	} else {
		log(VKM_LOG_LEVEL_INFO, vkm::std::vector<vkm_string>(vkm::std::move(tags)), strlen(fmt), const_cast<char*>(fmt));
	}
#endif
}
template <typename... T>
inline static void iPrintf([[maybe_unused]] const char* fmt, [[maybe_unused]] T... args) noexcept {
#if VKM_LOG_LEVEL <= VKM_LOG_LEVEL_INFO
	iPrintf<0, T...>({}, fmt, args...);
#endif
}
template <size_t N, typename... T>
inline static void wPrintf([[maybe_unused]] vkm::std::array<vkm_string, N>&& tags, [[maybe_unused]] const char* fmt,
						   [[maybe_unused]] T... args) noexcept {
#if VKM_LOG_LEVEL <= VKM_LOG_LEVEL_WARN
	if constexpr (sizeof...(T) > 0) {
		//+1 for null terminator
		const int n = snprintf(nullptr, 0, fmt, args...) + 1;
		vkm::std::vector<char> buf(n);
		snprintf(buf.get(), n, fmt, args...);

		//-1 to get strlen
		log(VKM_LOG_LEVEL_WARN, vkm::std::vector<vkm_string>(vkm::std::move(tags)), n - 1, buf.get());
	} else {
		log(VKM_LOG_LEVEL_WARN, vkm::std::vector<vkm_string>(vkm::std::move(tags)), strlen(fmt), const_cast<char*>(fmt));
	}
#endif
}
template <typename... T>
inline static void wPrintf([[maybe_unused]] const char* fmt, [[maybe_unused]] T... args) noexcept {
#if VKM_LOG_LEVEL <= VKM_LOG_LEVEL_WARN
	wPrintf<0, T...>({}, fmt, args...);
#endif
}
template <size_t N, typename... T>
inline static void ePrintf(vkm::std::array<vkm_string, N>&& tags, const char* fmt, T... args) noexcept {
	if constexpr (sizeof...(T) > 0) {
		//+1 for null terminator
		const int n = snprintf(nullptr, 0, fmt, args...) + 1;
		vkm::std::vector<char> buf(n);
		snprintf(buf.get(), n, fmt, args...);

		//-1 to get strlen
		log(VKM_LOG_LEVEL_ERROR, vkm::std::vector<vkm_string>(vkm::std::move(tags)), n - 1, buf.get());
	} else {
		log(VKM_LOG_LEVEL_ERROR, vkm::std::vector<vkm_string>(vkm::std::move(tags)), strlen(fmt), const_cast<char*>(fmt));
	}
}
template <typename... T>
inline static void ePrintf(const char* fmt, T... args) noexcept {
	ePrintf<0, T...>({}, fmt, args...);
}

inline static void fatal(const char* msg, vkm::std::sourceLocation loc = vkm::std::sourceLocation::current()) noexcept {
	ePrintf(vkm::std::array{VKM_MAKE_STRING("Fatal")}, "%s", msg);
	vkm::std::abort(msg, loc);
}
template <size_t N, typename... T>
inline static void fatal(vkm::std::sourceLocation loc, vkm::std::array<vkm_string, N>&& tags, const char* fmt, T... args) noexcept {
	const int n = snprintf(nullptr, 0, fmt, args...) + 1;
	vkm::std::vector<char> buf(n);
	snprintf(buf.get(), n, fmt, args...);
	if constexpr (sizeof...(T) > 0) {
		//+1 for null terminator
		const int n = snprintf(nullptr, 0, fmt, args...) + 1;
		vkm::std::vector<char> buf(n);
		snprintf(buf.get(), n, fmt, args...);

		//-1 to get strlen
		log(VKM_LOG_LEVEL_ERROR, vkm::std::vector<vkm_string>(vkm::std::move(tags), VKM_MAKE_STRING("Fatal")), n - 1, buf.get());
	} else {
		log(VKM_LOG_LEVEL_ERROR, vkm::std::vector<vkm_string>(vkm::std::move(tags), VKM_MAKE_STRING("Fatal")),
			strlen(fmt), const_cast<char*>(fmt));
	}
	vkm::std::abort(buf.get(), loc);
}
template <typename... T>
inline static void fatal(vkm::std::sourceLocation loc, const char* fmt, T... args) noexcept {
	fatal<0, T...>(loc, {}, fmt, args...);
}

}  // namespace vkm

#define VK_PROC(FN) VKM_VKFN(FN)
#ifndef NDEBUG
#define VK_DEBUG_PROC(FN) VKM_VKFN(FN)
#else
#define VK_DEBUG_PROC(FN)
#endif
