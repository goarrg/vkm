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
#include <concepts>

// NOLINTBEGIN(modernize-avoid-c-style-cast)

namespace vkm::std {
template <typename T, typename U>
constexpr void copy(T first, T last, U dst) noexcept {
	for (; first != last; ++first, ++dst) {
		*dst = *first;
	}
}
template <typename T>
	requires requires(T f) {
		{ f(size_t(0)) } -> ::std::same_as<bool>;
	}
constexpr size_t linearSearch(size_t n, T f) {
	for (size_t i = 0; i < n; i++) {
		if (f(i)) {
			return i;
		}
	}
	return n;
}
template <typename T>
	requires requires(T f) {
		{ f(size_t(0)) } -> ::std::same_as<bool>;
	}
constexpr size_t binarySearch(size_t n, T f) noexcept {
	size_t i = 0, j = n;
	while (i < j) {
		size_t h = (i + j) >> 1;
		if (!f(h)) {
			i = h + 1;
		} else {
			j = h;
		}
	}
	return i;
}
template <typename T>
	requires requires(T f) { f(size_t(0)) > 0; }
constexpr size_t binarySearch(size_t n, T f) noexcept {
	size_t i = 0, j = n;
	while (i < j) {
		size_t h = (i + j) >> 1;
		if (f(h) > 0) {
			i = h + 1;
		} else {
			j = h;
		}
	}
	return i;
}
}  // namespace vkm::std

// NOLINTEND(modernize-avoid-c-style-cast)
