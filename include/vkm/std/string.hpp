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
#include <string.h>

typedef struct {  // NOLINT(modernize-use-using)
	size_t len;
	const char* ptr;
} vkm_string;

#define VKM_MAKE_STRING(str) \
	vkm_string {             \
		strlen(str), str     \
	}

#ifdef __cplusplus
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <type_traits>

#include "stdlib.hpp"
#include "utility.hpp"

namespace vkm::std {
// clang-format off
template <typename T>
concept character =
	::std::is_same_v<T, char> ||
	// same_as<T, signed char> ||
	// same_as<T, unsigned char> ||
	::std::is_same_v<T, char8_t>;
	// same_as<T, char16_t> ||
	// same_as<T, char32_t>;
// clang-format on

template <character T>
constexpr size_t strlen(const T* const str) noexcept {
	if (str == nullptr) {
		return 0;
	}

	size_t sz = 0;
	for (; str[sz] != '\0'; ++sz) {
	}
	return sz;
}

template <character T>
constexpr size_t strnlen(const T* const str, const size_t n) noexcept {
	if (str == nullptr) {
		return 0;
	}

	size_t sz = 0;
	for (; sz <= n && str[sz] != '\0'; ++sz) {
	}
	return sz;
}

template <character T>
constexpr T* strncpy(T* dst, const T* src, size_t n) noexcept {
	if (!(src && dst)) {
		abort("Null args");
	}

	T* ptr = dst;						// NOLINT(misc-const-correctness)
	while (n-- && (*dst++ = *src++)) {	// NOLINT(clang-analyzer-core.NullDereference, clang-analyzer-security.ArrayBound)
	}
	return ptr;
}

template <character T = char>
class stringbuilder;

template <character T = char>
class string {
   private:
	friend stringbuilder<T>;

	size_t len = 0;
	size_t cap = 0;
	T* ptr = nullptr;

	void alloc(size_t n) noexcept {
		if (n == 0) {
			abort();
			return;
		}
		if (this->cap < n) {
			const size_t sz = (n + 1) * sizeof(T);
			T* newptr = static_cast<T*>(realloc(this->ptr, sz));
			if (newptr == nullptr) {
				abort("Failed realloc");
				return;
			}
			this->ptr = newptr;
			this->ptr[this->len] = 0;
			this->cap = n;
		}
	}

	void copy(size_t off, size_t n, const T* val) noexcept {
		if (n == 0) {
			return;
		}
		if (val == nullptr) {
			abort("Unexpected nullptr");
			return;
		}
		const size_t newLen = (off + n);
		alloc(newLen);
		if (this->ptr != nullptr) {
			memcpy(this->ptr + off, val, n * sizeof(T));
			this->len = newLen;
			this->ptr[this->len] = 0;
		}
	}

   public:
	constexpr string() noexcept = default;
	string(const T* val) noexcept { copy(0, strlen(val), val); }
	string(size_t n, const T* val) noexcept { copy(0, n, val); }
	string(::vkm_string val) noexcept { copy(0, val.len, val.ptr); }
	string(const string& other) noexcept { copy(0, other.len, other.ptr); }
	constexpr string(string&& other) noexcept { *this = vkm::std::move(other); }
	~string() noexcept {
		free(this->ptr);
		this->len = this->cap = 0;
		this->ptr = nullptr;
	}
	[[nodiscard]] constexpr operator const T*() const { return this->ptr; }
	[[nodiscard]] constexpr size_t size() const noexcept { return this->len; }
	[[nodiscard]] constexpr const T* cStr() const noexcept { return this->ptr; }
	[[nodiscard]] constexpr ::vkm_string vkm_string() const noexcept {
		return ::vkm_string{.len = this->len, .ptr = this->ptr};
	}

	void clear() noexcept {
		this->len = 0;
		this->ptr[this->len] = 0;
	}

	void assign(size_t n, const T* val) noexcept { copy(0, n, val); }
	string& operator=(const T* val) noexcept {
		assign(strlen(val), val);
		return *this;
	}
	string& operator=(::vkm_string val) noexcept {
		assign(val.len, val.ptr);
		return *this;
	}
	string& operator=(const string& val) noexcept {
		if (this != &val) {
			assign(val.len, val.ptr);
		}
		return *this;
	}
	constexpr string& operator=(string&& other) noexcept {
		this->len = other.len;
		this->cap = other.cap;
		this->ptr = other.ptr;

		other.len = other.cap = 0;
		other.ptr = nullptr;
		return *this;
	}

	string operator+(const T* val) noexcept {
		string newStr{};
		size_t n = strlen(val);
		newStr.alloc(this->len + n);
		newStr.copy(0, this->len, this->ptr);
		newStr.copy(this->len, n, val);
		return vkm::std::move(newStr);
	}
	string operator+(::vkm_string val) noexcept {
		string newStr{};
		newStr.alloc(this->len + val.len);
		newStr.copy(0, this->len, this->ptr);
		newStr.copy(this->len, val.len, val.ptr);
		return vkm::std::move(newStr);
	}
	string operator+(const string& val) noexcept {
		string newStr{};
		newStr.alloc(this->len + val.len);
		newStr.copy(0, this->len, this->ptr);
		newStr.copy(this->len, val.len, val);
		return vkm::std::move(newStr);
	}
	string operator+(string&& val) noexcept {
		string newStr{};
		newStr.alloc(this->len + val.len);
		newStr.copy(0, this->len, this->ptr);
		newStr.copy(this->len, val.len, val);
		return vkm::std::move(newStr);
	}

	[[nodiscard]] bool operator==(const string<T>& other) const {
		return strncmp(this->ptr, other, ::vkm::std::max(this->len, other.len)) == 0;
	}
	[[nodiscard]] bool operator!=(const string<T>& other) const {
		return strncmp(this->ptr, other, ::vkm::std::max(this->len, other.len)) != 0;
	}
	[[nodiscard]] bool operator==(const ::vkm_string& other) const {
		return strncmp(this->ptr, other.ptr, ::vkm::std::max(this->len, other.len)) == 0;
	}
	[[nodiscard]] bool operator!=(const ::vkm_string& other) const {
		return strncmp(this->ptr, other.ptr, ::vkm::std::max(this->len, other.len)) != 0;
	}
	template <character U>
	[[nodiscard]] bool operator==(const U* u) const {
		return strncmp(this->ptr, u, ::vkm::std::max(this->len, strlen(u))) == 0;
	}
	template <character U>
	[[nodiscard]] bool operator!=(const U* u) const {
		return strncmp(this->ptr, u, ::vkm::std::max(this->len, strlen(u))) != 0;
	}
};

template <character T>
class stringbuilder {
   private:
	size_t len = 0;
	size_t cap = 0;
	T* ptr = nullptr;

	void grow(size_t n) noexcept {
		static constexpr size_t blockSize = 256;
		this->cap = ((this->cap + n + blockSize - 1) / blockSize) * blockSize;
		const size_t sz = this->cap * sizeof(T);
		T* newptr = static_cast<T*>(realloc(this->ptr, sz));
		if (newptr == nullptr) {
			abort("Failed realloc");
			return;
		}
		this->ptr = newptr;
	}

   public:
	stringbuilder(const stringbuilder&) = delete;
	stringbuilder& operator=(const stringbuilder&) = delete;

	constexpr stringbuilder() noexcept = default;
	constexpr stringbuilder(stringbuilder&& other) noexcept { *this = vkm::std::move(other); }
	~stringbuilder() noexcept {
		free(this->ptr);
		this->len = this->cap = 0;
		this->ptr = nullptr;
	}

	[[nodiscard]] constexpr size_t size() const noexcept { return this->len; }

	[[nodiscard]] constexpr string<T> str() const noexcept { return vkm::std::move(string<T>(this->len, this->ptr)); }
	[[nodiscard]] constexpr const T* cStr() const noexcept {
		if (this->len != 0) {
			return this->ptr;
		}
		return nullptr;
	}
	[[nodiscard]] constexpr ::vkm_string vkm_string() const noexcept {
		if (this->len != 0) {
			return ::vkm_string{.len = this->len, .ptr = this->ptr};
		}
		return ::vkm_string{};
	}

	stringbuilder& reset() noexcept {
		this->len = 0;
		this->ptr[this->len] = 0;
		return *this;
	}

	stringbuilder& write(const size_t valueLen, const T* value) noexcept {
		if (valueLen == 0) {
			return *this;
		}

		const ptrdiff_t space = (this->cap - this->len);
		const ptrdiff_t diff = valueLen + 1 - space;
		if (diff > 0) {
			grow(diff);
		}
		if (this->ptr == nullptr) {
			abort("Unexpected nullptr");
			return *this;
		}
		strncpy(this->ptr + this->len, value, valueLen);
		this->len += valueLen;
		// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
		this->ptr[this->len] = 0;
		return *this;
	}
	stringbuilder& write(::vkm_string value) noexcept { return this->write(value.len, value.ptr); }
	stringbuilder& write(const string<T>&& value) noexcept { return this->write(value.size(), value.cStr()); }
	stringbuilder& write(const T* value) noexcept {
		const size_t len = strlen(value);
		return this->write(len, value);
	}
	template <typename... Args>
	stringbuilder& write(const T* fmt, Args... args) noexcept {
		const ptrdiff_t space = (this->cap - this->len);
		const size_t n = snprintf(this->ptr + this->len, space, fmt, args...) + 1;
		const ptrdiff_t diff = n - space;
		if (diff > 0) {
			grow(diff);
			snprintf(this->ptr + this->len, n, fmt, args...);
		}
		this->len += n - 1;
		return *this;
	}
	stringbuilder& backspace(size_t n = 1) noexcept {
		if (this->len == 0) {
			return *this;
		}
		if (this->ptr == nullptr) {
			abort("Unexpected nullptr");
			return *this;
		}
		this->len -= min(n, this->len);
		this->ptr[this->len] = 0;
		return *this;
	}

	stringbuilder& operator=(stringbuilder&& other) noexcept {
		this->len = other.len;
		this->cap = other.cap;
		this->ptr = other.ptr;

		other.len = other.cap = 0;
		other.ptr = nullptr;

		return *this;
	}

	stringbuilder& operator<<(const T* value) noexcept { return write(value); }
	stringbuilder& operator<<(::vkm_string value) noexcept { return write(value); }
	stringbuilder& operator<<(const string<T>&& value) noexcept { return write(value); }

	stringbuilder& operator<<(char value) noexcept { return write("%hhd", value); }
	stringbuilder& operator<<(unsigned char value) noexcept { return write("%hhu", value); }

	stringbuilder& operator<<(short value) noexcept { return write("%hd", value); }
	stringbuilder& operator<<(unsigned short value) noexcept { return write("%hu", value); }

	stringbuilder& operator<<(int value) noexcept { return write("%d", value); }
	stringbuilder& operator<<(unsigned int value) noexcept { return write("%u", value); }

	stringbuilder& operator<<(long value) noexcept { return write("%ld", value); }
	stringbuilder& operator<<(unsigned long value) noexcept { return write("%lu", value); }

	stringbuilder& operator<<(long long value) noexcept { return write("%lld", value); }
	stringbuilder& operator<<(unsigned long long value) noexcept { return write("%llu", value); }

	stringbuilder& operator<<(float value) noexcept { return write("%f", value); }
	stringbuilder& operator<<(double value) noexcept { return write("%lf", value); }
	stringbuilder& operator<<(long double value) noexcept { return write("%Lf", value); }

	stringbuilder& operator<<(bool value) noexcept {
		return value ? this << static_cast<const T*>("true") : this << static_cast<const T*>("false");
	}
};
}  // namespace vkm::std
#endif
