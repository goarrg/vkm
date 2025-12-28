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
#include <new>	// IWYU pragma: keep
#include <concepts>
#include <algorithm>

#include "stdlib.hpp"
#include "utility.hpp"
#include "array.hpp"
#include "defer.hpp"

namespace vkm::std {
template <typename T>
class vector {
   private:
	size_t len = 0;
	size_t cap = 0;
	T* ptr = nullptr;

	static_assert(::std::is_pointer_v<T> || (::std::default_initializable<T> && ::std::destructible<T>));

	void clear() noexcept {
		if constexpr (::std::destructible<T>) {
			for (size_t i = 0; i < this->len; i++) {
				this->ptr[i].~T();
			}
		}
		this->len = 0;
	}

	void grow(size_t n) noexcept {
		const size_t targetCap = this->cap + n;
		const size_t doubleCap = this->cap + this->cap;

		if (targetCap > doubleCap) {
			return reserve(targetCap);
		}

		static constexpr size_t threshold = 256;
		if (targetCap < threshold) {
			return reserve(doubleCap);
		}

		size_t newCap = this->cap;
		do {
			newCap += (newCap + (3 * threshold)) >> 2;
		} while (newCap < targetCap);

		return reserve(newCap);
	}

   public:
	vector(const vector&) = delete;
	vector& operator=(const vector&) = delete;

	constexpr vector() noexcept = default;
	template <size_t N>
	constexpr vector(vkm::std::array<T, N>&& args) noexcept {
		*this = vkm::std::move(args);
	}
	template <size_t N, typename... U>
	constexpr vector(vkm::std::array<T, N>&& args, U&&... others) noexcept {
		this->reserve(N + sizeof(U)...);
		*this = vkm::std::move(args);
		for (auto& o : {others...}) {
			this->pushBack(vkm::std::move(o));
		}
	}
	constexpr vector(vkm::std::array<T, 0>&&) noexcept {}
	template <typename... U>
	constexpr vector(vkm::std::array<T, 0>&& args, U&&... others) noexcept {
		*this = vkm::std::move(args);
		for (auto& o : {others...}) {
			this->pushBack(vkm::std::move(o));
		}
	}
	template <typename U>
	constexpr vector(const vector<U>& val) noexcept
		requires ::std::convertible_to<U, T>
	{
		*this = val;
	}
	template <typename U>
	vector(size_t len, const vector<U>& val) noexcept
		requires ::std::convertible_to<U, T>
	{
		reserve(len);
		for (size_t i = 0; i < len; i++) {
			pushBack(val[i]);
		}
	}
	constexpr vector(vector&& val) noexcept { *this = vkm::std::move(val); }
	vector(size_t len) noexcept : vector(len, len) {}
	vector(size_t len, size_t cap) noexcept {
		if (cap < len) {
			abort("cap < len");
		}
		reserve(cap);
		resize(len);
	}
	vector(size_t len, size_t cap, T val) noexcept {
		if (cap < len) {
			abort("cap < len");
		}
		reserve(cap);
		resize(len, val);
	}
	~vector() noexcept {
		clear();
		free(reinterpret_cast<void*>(this->ptr));
		this->len = this->cap = 0;
		this->ptr = nullptr;
	}

	void reserve(size_t n) noexcept {
		if (this->cap >= n) {
			return;
		}
		const size_t sz = n * sizeof(T);  // NOLINT(bugprone-sizeof-expression)
		T* newptr = static_cast<T*>(realloc(reinterpret_cast<void*>(this->ptr), sz));
		if (newptr == nullptr) {
			abort("Failed realloc");
		}
		this->cap = n;
		this->ptr = newptr;
	}
	void resize(size_t n) noexcept {
		if constexpr (::std::destructible<T>) {
			for (size_t i = n; i < this->len; i++) {
				this->ptr[i].~T();
			}
		}
		reserve(n);
		if constexpr (::std::default_initializable<T>) {
			for (size_t i = this->len; i < n; i++) {
				new (reinterpret_cast<void*>(&this->ptr[i])) T();
			}
		}
		this->len = n;
	}
	void resize(size_t n, T value) noexcept
		requires ::std::copy_constructible<T>
	{
		if constexpr (::std::destructible<T>) {
			for (size_t i = n; i < this->len; i++) {
				this->ptr[i].~T();
			}
		}
		reserve(n);
		for (size_t i = this->len; i < n; i++) {
			new (reinterpret_cast<void*>(&this->ptr[i])) T(value);
		}
		this->len = n;
	}

	T& operator[](size_t i) noexcept {
		if ((len == 0) || !inRange<size_t>(i, 0, len - 1)) {
			abort("Index out of bounds");
		}

		return ptr[i];
	}
	const T& operator[](size_t i) const noexcept {
		if ((len == 0) || !inRange<size_t>(i, 0, len - 1)) {
			abort("Index out of bounds");
		}

		return ptr[i];
	}

	template <typename U>
	vector& operator=(const vector<U>& other) noexcept
		requires ::std::convertible_to<U, T>
	{
		this->clear();
		this->reserve(other.size());
		for (auto& arg : other) {
			new (reinterpret_cast<void*>(&this->ptr[this->len++])) T(arg);
		}
		return *this;
	}

	template <size_t N>
	vector& operator=(vkm::std::array<T, N>&& args) noexcept {
		this->clear();
		this->reserve(N);
		for (auto& arg : args) {
			new (reinterpret_cast<void*>(&this->ptr[this->len++])) T(vkm::std::move(arg));
		}
		return *this;
	}
	vector& operator=(vector&& other) noexcept {
		this->len = other.len;
		this->cap = other.cap;
		this->ptr = other.ptr;

		other.len = other.cap = 0;
		other.ptr = nullptr;

		return *this;
	}

	[[nodiscard]] size_t size() const noexcept { return len; }
	[[nodiscard]] size_t capacity() const noexcept { return cap; }

	[[nodiscard]] const T* get() const noexcept { return ptr; }
	[[nodiscard]] const T* begin() const noexcept { return ptr; }
	[[nodiscard]] const T* end() const noexcept { return begin() + size(); }
	[[nodiscard]] const T& first() const noexcept { return this->ptr[0]; }
	[[nodiscard]] const T& last() const noexcept { return this->ptr[size() - 1]; }

	[[nodiscard]] T* get() noexcept { return ptr; }
	[[nodiscard]] T* begin() noexcept { return ptr; }
	[[nodiscard]] T* end() noexcept { return begin() + size(); }
	[[nodiscard]] T& first() noexcept { return this->ptr[0]; }
	[[nodiscard]] T& last() noexcept { return this->ptr[size() - 1]; }

	void pushBack(T&& val) noexcept {
		if (this->len == this->cap) {
			grow(1);
		}
		new (reinterpret_cast<void*>(&this->ptr[this->len++])) T(vkm::std::move(val));
	}
	void pushBack(const T& val) noexcept {
		if (this->len == this->cap) {
			grow(1);
		}
		new (reinterpret_cast<void*>(&this->ptr[this->len++])) T(val);
	}
	void pushBack(size_t n, const T* val) noexcept {
		if (n == 0) {
			return;
		}
		reserve(this->len + n);
		for (size_t i = 0; i < n; i++) {
			new (reinterpret_cast<void*>(&this->ptr[this->len++])) T(val[i]);
		}
	}

	void popBack() noexcept {
		if (this->len > 0) {
			this->len -= 1;
			if constexpr (::std::destructible<T>) {
				this->ptr[this->len].~T();
			}
		}
	}

	T dequeueBack() noexcept {
		if (this->len == 0u) {
			abort("Empty vector");
		}

		DEFER([&] {
			if constexpr (::std::destructible<T>) {
				this->ptr[this->len].~T();
			};
		});
		return vkm::std::move(this->ptr[--this->len]);
	}

	template <typename U>
	bool contains(const U& want) noexcept
		requires ::std::equality_comparable<T>
	{
		for (auto& have : *this) {
			if (have == want) {
				return true;
			}
		}
		return false;
	}

	void compact() noexcept
		requires ::std::equality_comparable<T>
	{
		if (this->size() <= 1) {
			return;
		}
		size_t newSize = 1;
		for (size_t i = 1; i < this->size(); i++) {
			if ((*this)[i] == (*this)[i - 1]) {
				for (size_t j = ++i; j < this->size(); i++, j++) {
					if ((*this)[j] != (*this)[j - 1]) {
						(*this)[newSize++] = (*this)[j];
						break;
					}
				}
			} else {
				(*this)[newSize++] = (*this)[i];
			}
		}
		this->resize(newSize);
	}

	void sortComptact() noexcept
		requires ::std::equality_comparable<T>
	{
		::std::ranges::sort(*this);
		this->compact();
	}
};
}  // namespace vkm::std
