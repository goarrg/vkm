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
#include <stdlib.h>
#include <string.h>
#include <new>	// IWYU pragma: keep
#include <concepts>

#include "stdlib.hpp"
#include "defer.hpp"
#include "utility.hpp"

namespace vkm::std {
template <typename T>
class ringbuffer {
   private:
	size_t start = 0;
	size_t len = 0;
	size_t cap = 0;
	T* ptr = nullptr;

	void clear() noexcept {
		if constexpr (::std::destructible<T>) {
			for (size_t i = 0; i < this->len; i++) {
				this->ptr[(this->start + i) % this->cap].~T();
			}
		}
		this->start = this->len = 0;
	}

	void grow(size_t n) noexcept { reserve(((this->cap + n) * 3 / 2)); }

   public:
	ringbuffer(const ringbuffer&) = delete;
	ringbuffer& operator=(const ringbuffer&) = delete;

	constexpr ringbuffer() noexcept = default;
	constexpr ringbuffer(ringbuffer&& other) noexcept { *this = vkm::std::move(other); }
	ringbuffer(size_t cap) noexcept { reserve(cap); }
	~ringbuffer() noexcept {
		clear();
		free(reinterpret_cast<void*>(this->ptr));
		this->cap = 0;
		this->ptr = nullptr;
	}

	void reserve(size_t n) noexcept {
		if (this->cap >= n) {
			return;
		}
		size_t sz = n * sizeof(T);	// NOLINT(bugprone-sizeof-expression)
		T* newptr = static_cast<T*>(realloc(reinterpret_cast<void*>(this->ptr), sz));
		if (newptr == nullptr) {
			abort("Failed realloc");
		}
		this->ptr = newptr;
		if (this->len != 0u) {
			const size_t end = (this->start + this->len - 1) % this->cap;
			if (this->start > end) {
				const size_t shift = n - this->cap;
				sz = (this->cap - this->start) * sizeof(T);	 // NOLINT(bugprone-sizeof-expression)
				// NOLINTNEXTLINE(clang-analyzer-core.NullDereference, clang-analyzer-unix.cstring.NullArg)
				memmove(reinterpret_cast<void*>(this->ptr + this->start + shift),
						reinterpret_cast<void*>(this->ptr + this->start), sz);
				this->start += shift;
			}
		}
		this->cap = n;
	}

	ringbuffer& operator=(ringbuffer&& other) noexcept {
		this->start = other.start;
		this->len = other.len;
		this->cap = other.cap;
		this->ptr = other.ptr;

		other.start = other.len = other.cap = 0;
		other.ptr = nullptr;

		return *this;
	}

	T& operator[](size_t i) noexcept {
		if (!inRange<size_t>(i, 0, len - 1)) {
			abort("Index out of bounds");
		}

		return ptr[(this->start + i) % this->cap];
	}
	const T& operator[](size_t i) const noexcept {
		if (!inRange<size_t>(i, 0, len - 1)) {
			abort("Index out of bounds");
		}

		return ptr[(this->start + i) % this->cap];
	}

	[[nodiscard]] size_t size() const noexcept { return len; }
	[[nodiscard]] size_t capacity() const noexcept { return cap; }
	[[nodiscard]] const T* get() const noexcept { return ptr; }

	[[nodiscard]] T* get() noexcept { return ptr; }

	void pushBack(T&& val) noexcept {
		if (this->len == this->cap) {
			grow(1);
		}
		new (reinterpret_cast<void*>(&this->ptr[(this->start + this->len++) % this->cap])) T(move(val));
	}
	void pushBack(const T& val) noexcept {
		if (this->len == this->cap) {
			grow(1);
		}
		new (reinterpret_cast<void*>(&this->ptr[(this->start + this->len++) % this->cap])) T(val);
	}

	T dequeueFront() noexcept {
		if (this->len == 0u) {
			abort("Empty buffer");
		}

		DEFER([&] {
			if constexpr (::std::destructible<T>) {
				this->ptr[this->start].~T();
			}
			this->start = (this->start + 1) % this->cap;
			this->len = this->len - 1;
		});
		return vkm::std::move(this->ptr[this->start]);
	}
};
}  // namespace vkm::std
