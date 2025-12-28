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
#include <type_traits>

namespace vkm::std {
// NOLINTBEGIN(modernize-avoid-c-arrays)
template <typename T>
struct defaultDeleter {
	constexpr defaultDeleter() noexcept = default;
	constexpr void operator()(T* ptr) const noexcept { delete ptr; }
};
template <typename T>
struct defaultDeleter<T[]> {
	constexpr defaultDeleter() noexcept = default;
	constexpr void operator()(T* ptr) const noexcept { delete[] ptr; }
};
template <>
struct defaultDeleter<void> {
	constexpr defaultDeleter() noexcept = default;
	void operator()(void* ptr) const noexcept { ::operator delete(ptr); }
};

namespace internal {
template <typename T>
class smartPtrBase {
   protected:
	using pointer = ::std::remove_extent_t<T>*;

	pointer ptr = nullptr;
	void (*deleter)(pointer) = [](pointer ptr) noexcept { defaultDeleter<T>()(ptr); };

   public:
	smartPtrBase(const smartPtrBase&) = delete;
	smartPtrBase& operator=(const smartPtrBase&) = delete;

	constexpr smartPtrBase() noexcept = default;
	constexpr smartPtrBase(pointer ptr) noexcept : ptr(ptr) {}
	constexpr smartPtrBase(pointer ptr, void (*deleter)(pointer)) noexcept : ptr(ptr), deleter(deleter) {}
	constexpr smartPtrBase(smartPtrBase&& other) noexcept {
		this->ptr = other.ptr;
		this->deleter = other.deleter;
		other.ptr = nullptr;
	}
	~smartPtrBase() noexcept { this->deleter(ptr); }

	constexpr smartPtrBase& operator=(smartPtrBase&& other) noexcept {
		this->deleter(this->ptr);
		this->ptr = other.ptr;
		this->deleter = other.deleter;
		other.ptr = nullptr;
		return *this;
	}
	constexpr smartPtrBase& operator=(pointer ptr) noexcept {
		this->deleter(this->ptr);
		this->ptr = ptr;
		return *this;
	}

	[[nodiscard]] constexpr pointer get() noexcept { return ptr; }
};
}  // namespace internal

template <typename T>
class smartPtr : public internal::smartPtrBase<T> {
	using pointer = internal::smartPtrBase<T>::pointer;

   public:
	using internal::smartPtrBase<T>::smartPtrBase;

	[[nodiscard]] constexpr T& operator*() noexcept { return *this->ptr; }
	[[nodiscard]] constexpr pointer operator->() noexcept { return this->ptr; }
};

template <>
class smartPtr<void> : public internal::smartPtrBase<void> {
   public:
	using smartPtrBase<void>::smartPtrBase;
};

template <typename T>
class smartPtr<T[]> : public internal::smartPtrBase<T[]> {
   public:
	using internal::smartPtrBase<T[]>::smartPtrBase;

	[[nodiscard]] constexpr T& operator[](size_t i) noexcept { return this->ptr[i]; }
};
// NOLINTEND(modernize-avoid-c-arrays)
}  // namespace vkm::std
