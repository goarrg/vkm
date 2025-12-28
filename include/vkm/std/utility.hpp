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

#include <type_traits>

#include "concepts.hpp"

namespace vkm::std {
// NOLINTBEGIN(readability-braces-around-statements)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
template <integral T, integral U>
[[nodiscard]] constexpr bool cmpBitFlagsContains(T t, U u) noexcept {
	return (t & u) == u;
}
template <integral T, integral U>
[[nodiscard]] constexpr bool cmpBitFlagsNotContains(T t, U u) noexcept {
	return (t & u) == 0;
}
template <integral T, integral U, integral V>
[[nodiscard]] constexpr bool cmpBitFlags(T t, U want, V dontWant) noexcept {
	return ((t & want) == want) && ((t & dontWant) == 0);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool cmpEqual(T t, U u) noexcept {
	if constexpr (::std::is_signed_v<T> == ::std::is_signed_v<U>)
		return t == u;
	else if constexpr (::std::is_signed_v<T>)
		return t < 0 ? false : t == u;
	else
		return u < 0 ? false : t == u;
}

template <typename T, typename U>
[[nodiscard]] constexpr bool cmpNotEqual(T t, U u) noexcept {
	return !cmp_equal(t, u);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool cmpLess(T t, U u) noexcept {
	if constexpr (::std::is_signed_v<T> == ::std::is_signed_v<U>)
		return t < u;
	else if constexpr (::std::is_signed_v<T>)
		return t < 0 ? true : t < u;
	else
		return u < 0 ? false : t < u;
}
#pragma clang diagnostic pop
// NOLINTEND(readability-braces-around-statements)

template <typename T, typename U>
[[nodiscard]] constexpr bool cmpGreater(T t, U u) noexcept {
	return cmpLess(u, t);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool cmpLessEqual(T t, U u) noexcept {
	return !cmpGreater(t, u);
}

template <typename T, typename U>
[[nodiscard]] constexpr bool cmpGreaterEqual(T t, U u) noexcept {
	return !cmpLess(t, u);
}

template <typename T>
[[nodiscard]] constexpr bool inRange(T t, T lo, T hi) noexcept {
	return cmpGreaterEqual(t, lo) && cmpLessEqual(t, hi);
}

template <typename T>
[[nodiscard]] constexpr T min(T x, T y) noexcept {
	return cmpLess(x, y) ? x : y;
}

template <typename T>
[[nodiscard]] constexpr T max(T x, T y) noexcept {
	return cmpGreater(x, y) ? x : y;
}

template <typename T>
[[nodiscard]] constexpr T clamp(T t, T lo, T hi) noexcept {
	return max(lo, min(t, hi));
}

template <typename T, typename U>
constexpr void swap(T& a, U& b) noexcept {
	if (&a == &b) {
		return;
	}
	T c(a);
	a = b;
	b = c;
}

template <typename T>
[[nodiscard]] constexpr ::std::remove_reference_t<T>&& move(T&& t) noexcept {
	return static_cast<::std::remove_reference_t<T>&&>(t);
}

template <typename T, typename U>
struct pair {
	T first = T();
	U second = U();

	constexpr pair() noexcept = default;
	constexpr pair(T& x, U& y) noexcept : first(x), second(y) {}
	constexpr pair(T&& x, U&& y) noexcept : first(vkm::std::move(x)), second(vkm::std::move(y)) {}
	constexpr pair(T& x, U&& y) noexcept : first(x), second(vkm::std::move(y)) {}
	constexpr pair(T&& x, U& y) noexcept : first(vkm::std::move(x)), second(y) {}
};
template <typename T, typename U>
pair(T, U) -> pair<::std::remove_reference_t<T>, ::std::remove_reference_t<U>>;

template <typename T, typename U, typename V>
struct triple {
	T first = T();
	U second = U();
	V third = V();

	constexpr triple() noexcept = default;
	constexpr triple(T& x, U& y, V& z) noexcept : first(x), second(y), third(z) {}
	constexpr triple(T&& x, U&& y, V&& z) noexcept
		: first(vkm::std::move(x)), second(vkm::std::move(y)), third(vkm::std::move(z)) {}
};
template <typename T, typename U, typename V>
triple(T, U, V) -> triple<::std::remove_reference_t<T>, ::std::remove_reference_t<U>, ::std::remove_reference_t<V>>;
}  // namespace vkm::std
