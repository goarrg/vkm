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

#include "utility.hpp"
#include "stdlib.hpp"

namespace vkm::std {
template <typename T, size_t N>
class array {
   private:
	static_assert(N > 0);
	T data[N] = {};	 // NOLINT(modernize-avoid-c-arrays)

   public:
	constexpr array() noexcept = default;
	constexpr array(const T (&data)[N]) noexcept {	// NOLINT(modernize-avoid-c-arrays)
		for (size_t i = 0; i < N; i++) {
			this->data[i] = data[i];
		}
	}

	template <typename... Args>
	constexpr array(Args&&... args) noexcept
		requires(::std::convertible_to<Args, T> && ...)
		: data{vkm::std::move(args)...} {
		static_assert(sizeof...(Args) == N);
	}

	void fill(const T& val) noexcept {
		for (size_t i = 0; i < N; i++) {
			data[i] = val;
		}
	}

	[[nodiscard]] constexpr size_t size() const noexcept { return N; }
	[[nodiscard]] constexpr const T* get() const noexcept { return &data[0]; }
	[[nodiscard]] constexpr const T* begin() const noexcept { return &data[0]; }
	[[nodiscard]] constexpr const T* end() const noexcept { return begin() + size(); }

	[[nodiscard]] T* get() noexcept { return &data[0]; }
	[[nodiscard]] T* begin() noexcept { return &data[0]; }
	[[nodiscard]] T* end() noexcept { return begin() + size(); }

	[[nodiscard]] T& operator[](size_t i) noexcept {
		if (!inRange<size_t>(i, 0, N - 1)) {
			abort("Index out of bounds");
		}

		return data[i];
	}
	[[nodiscard]] constexpr const T& operator[](size_t i) const noexcept {
		if (!inRange<size_t>(i, 0, N - 1)) {
			abort("Index out of bounds");
		}

		return data[i];
	}
};
template <typename T>
class array<T, 0> {
   public:
	constexpr array() noexcept = default;

	[[nodiscard]] constexpr size_t size() const noexcept { return 0; }
	[[nodiscard]] constexpr const T* get() const noexcept { return nullptr; }
	[[nodiscard]] constexpr const T* begin() const noexcept { return nullptr; }
	[[nodiscard]] constexpr const T* end() const noexcept { return nullptr; }

	[[nodiscard]] T* get() noexcept { return nullptr; }
	[[nodiscard]] T* begin() noexcept { return nullptr; }
	[[nodiscard]] T* end() noexcept { return nullptr; }

	[[nodiscard]] T operator[](size_t) noexcept {
		if constexpr (::std::is_pointer_v<T>) {
			return nullptr;
		}
		return T();
	}
	[[nodiscard]] constexpr T operator[](size_t) const noexcept {
		if constexpr (::std::is_pointer_v<T>) {
			return nullptr;
		}
		return T();
	}
};

template <typename T, typename... Ts>
array(T, Ts...) -> array<T, sizeof...(Ts) + 1>;
}  // namespace vkm::std
