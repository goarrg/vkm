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

#include "vkm/vkm.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <new>	// IWYU pragma: keep
#include <concepts>

#include "vkm/std/stdlib.hpp"
#include "vkm/std/string.hpp"
#include "vkm/std/array.hpp"
#include "vkm/std/vector.hpp"
#include "vkm/std/memory.hpp"
#include "vkm/std/utility.hpp"

namespace vkm::vk::reflect {
struct vkStructureChain {
	VkStructureType sType;
	vkStructureChain* pNext;
};
struct type {
   public:
	enum id : uint8_t {
		vkStructureType,
		voidPtr,
		vkBool32,
		maxID,
	};

   private:
	static constexpr vkm::std::array<const char*, maxID> names = {
		"VkStructureType",
		"VoidPtr",
		"VkBool32",
	};

   public:
	id id;

	constexpr type() noexcept = default;
	constexpr type(enum id id) noexcept : id(id) {}
	virtual constexpr ~type() noexcept = default;

	[[nodiscard]] constexpr const char* name() const { return names[id]; }
	[[nodiscard]] constexpr size_t size() const {
		switch (this->id) {
			case id::vkStructureType:
				return sizeof(vkStructureType);
			case id::voidPtr:
				return sizeof(void*);
			case id::vkBool32:
				return sizeof(vkBool32);

			case id::maxID:
				break;
		}

		vkm::std::abort();
		return 0;
	}
};
struct value {
	void* ptr = nullptr;

	constexpr value() noexcept = default;
	constexpr value(void* ptr) noexcept : ptr(ptr) {}
	virtual constexpr ~value() noexcept = default;
};

struct structField {
	::vkm::vk::reflect::type type;
	size_t offset;
	const char* name;

	constexpr structField() noexcept = default;
	constexpr structField(struct type type, size_t offset, const char* name) noexcept
		: type(vkm::std::move(type)), offset(offset), name(name) {};
	virtual constexpr ~structField() noexcept = default;
};
struct structType {
	const char* name;
	size_t size;

	constexpr structType(const char* name, size_t size) noexcept : name(name), size(size) {}
	virtual constexpr ~structType() noexcept = default;

	[[nodiscard]] virtual constexpr size_t numDependencies() const noexcept = 0;
	[[nodiscard]] virtual constexpr const char* dependency(size_t i) const noexcept = 0;

	[[nodiscard]] virtual constexpr size_t numFields() const noexcept = 0;
	[[nodiscard]] virtual constexpr structField field(size_t i) const noexcept = 0;
	[[nodiscard]] virtual constexpr const structField* begin() const noexcept = 0;
	[[nodiscard]] virtual constexpr const structField* end() const noexcept = 0;
	[[nodiscard]] virtual vkm::std::smartPtr<vkStructureChain> allocate() const noexcept = 0;
};
// NOLINTNEXTLINE(misc-multiple-inheritance)
struct structFieldValue : structField, value {
	constexpr structFieldValue() noexcept = default;
	constexpr structFieldValue(const structField& type, void* ptr) noexcept : structField(type), value{ptr} {}
	constexpr ~structFieldValue() noexcept override = default;
};
struct structValue : value {
	const structType* type;

	constexpr structValue() noexcept = default;
	constexpr structValue(const structType* type, void* ptr) noexcept : value{ptr}, type(type) {}
	constexpr ~structValue() noexcept override = default;

	[[nodiscard]] virtual constexpr size_t numFields() const noexcept = 0;
	[[nodiscard]] virtual constexpr structFieldValue field(size_t i) noexcept = 0;
	[[nodiscard]] virtual constexpr structFieldValue* begin() noexcept = 0;
	[[nodiscard]] virtual constexpr structFieldValue* end() noexcept = 0;
	[[nodiscard]] virtual vkm::std::smartPtr<vkStructureChain> clone() const noexcept = 0;
};

namespace internal {
template <size_t D, size_t N>
struct structTypeImpl : structType {
   private:
	const vkm::std::array<const char*, D> dependencies;
	const vkm::std::array<structField, N> fields;

   public:
	template <typename... Args>
		requires(::std::same_as<Args, structField> && ...)
	constexpr structTypeImpl(const char* name, size_t size, Args&&... fields) noexcept
		requires(D == 0)
		: structType(name, size), fields(vkm::std::move(fields)...) {
		static_assert(sizeof...(Args) == N);
	}
	constexpr structTypeImpl(const char* name, size_t size, auto dependencies, auto fields) noexcept
		: structType(name, size), dependencies(vkm::std::move(dependencies)), fields(vkm::std::move(fields)) {}
	constexpr ~structTypeImpl() noexcept override = default;

	[[nodiscard]] constexpr size_t numDependencies() const noexcept override { return D; }
	[[nodiscard]] constexpr const char* dependency(size_t i) const noexcept override { return dependencies[i]; }

	[[nodiscard]] constexpr size_t numFields() const noexcept override { return N; }
	[[nodiscard]] constexpr structField field(size_t i) const noexcept override { return fields[i]; }
	[[nodiscard]] constexpr const structField* begin() const noexcept override { return &fields[0]; }
	[[nodiscard]] constexpr const structField* end() const noexcept override { return this->begin() + N; }
	[[nodiscard]] vkm::std::smartPtr<vkStructureChain> allocate() const noexcept override {
		auto* tmp = calloc(1, size);
		if (tmp == nullptr) {
			vkm::std::abort();
		}
		return {static_cast<vkStructureChain*>(tmp), [](vkStructureChain* ptr) { free(ptr); }};
	}
};
template <size_t D, size_t N>
struct structChainTypeImpl : structTypeImpl<D, N> {
   private:
	VkStructureType sType;

   public:
	template <typename... Args>
		requires(::std::same_as<Args, structField> && ...)
	constexpr structChainTypeImpl(VkStructureType sType, const char* name, size_t size, Args&&... fields) noexcept
		requires(D == 0)
		: structTypeImpl<0, N>(name, size, vkm::std::move(fields)...), sType(sType) {
		static_assert(sizeof...(Args) == N);
	}
	constexpr structChainTypeImpl(VkStructureType sType, const char* name, size_t size,
								  vkm::std::array<const char*, D>&& dependencies, vkm::std::array<structField, N>&& fields) noexcept
		: structTypeImpl<D, N>(name, size, vkm::std::move(dependencies), vkm::std::move(fields)), sType(sType) {}
	constexpr ~structChainTypeImpl() noexcept override = default;

	[[nodiscard]] vkm::std::smartPtr<vkStructureChain> allocate() const noexcept override {
		auto tmp = structTypeImpl<D, N>::allocate();
		tmp.get()->sType = this->sType;
		return tmp;
	}
};
template <size_t N>
struct structValueImpl : structValue {
   private:
	vkm::std::array<structFieldValue, N> fields;

	[[nodiscard]] vkm::std::array<structFieldValue, N> makeFields() const noexcept {
		vkm::std::array<structFieldValue, N> fields;
		for (size_t i = 0; i < N; i++) {
			auto t = type->field(i);
			fields[i] = structFieldValue(t, (reinterpret_cast<uint8_t*>(ptr)) + t.offset);
		}
		return fields;
	}

   public:
	structValueImpl(const structType* type, void* ptr) noexcept : structValue(type, ptr), fields(makeFields()) {}
	constexpr ~structValueImpl() noexcept override = default;
	[[nodiscard]] constexpr size_t numFields() const noexcept override { return N; }
	[[nodiscard]] constexpr structFieldValue field(size_t i) noexcept override { return fields[i]; };
	[[nodiscard]] constexpr structFieldValue* begin() noexcept override { return &fields[0]; }
	[[nodiscard]] constexpr structFieldValue* end() noexcept override { return this->begin() + N; }
	[[nodiscard]] vkm::std::smartPtr<vkStructureChain> clone() const noexcept override {
		auto tmp = this->type->allocate();
		memcpy(tmp.get(), this->ptr, this->type->size);
		return vkm::std::move(tmp);
	}
};
}  // namespace internal

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#include "vkm/inc/reflect_struct.inc"  // IWYU pragma: keep
#pragma GCC diagnostic pop

[[nodiscard]] inline static auto cloneVkStructureChain(const vkStructureChain* ptr) noexcept {
	vkm::std::vector<vkm::std::smartPtr<vkStructureChain>> chain;

	auto clone = [&]() {
		const size_t sz = vkm::vk::reflect::sizeOf(ptr->sType);
		if (sz == 0) {
			vkm::std::stringbuilder builder;
			builder << "Unknown sType: " << ptr->sType;
			vkm::std::abort(builder.cStr());
		}
		auto* tmp = malloc(sz);
		if (tmp == nullptr) {
			vkm::std::abort();
		}
		memcpy(tmp, ptr, sz);
		chain.pushBack({static_cast<vkStructureChain*>(tmp), [](vkStructureChain* ptr) { free(ptr); }});
		chain.last()->pNext = nullptr;
	};
	if (ptr != nullptr) {
		clone();
		for (ptr = ptr->pNext; ptr != nullptr; ptr = ptr->pNext) {
			clone();
			chain[chain.size() - 2]->pNext = chain.last().get();
		}
	}

	return vkm::std::move(chain);
}
inline static void appendVkStructureChain(vkm::std::vector<vkm::std::smartPtr<vkStructureChain>>& chain, bool alloc,
										  vkStructureChain* ptr) noexcept {
	if (ptr == nullptr) {
		return;
	}
	auto clone = [&]() {
		if (alloc) {
			const size_t sz = vkm::vk::reflect::sizeOf(ptr->sType);
			if (sz == 0) {
				vkm::std::stringbuilder builder;
				builder << "Unknown sType: " << ptr->sType;
				vkm::std::abort(builder.cStr());
			}
			auto* tmp = malloc(sz);
			if (tmp == nullptr) {
				vkm::std::abort();
			}
			memcpy(tmp, ptr, sz);
			chain.pushBack({static_cast<vkStructureChain*>(tmp), [](vkStructureChain* ptr) { free(ptr); }});
		} else {
			// we need every pointer inside of chain so append works more than once
			chain.pushBack({ptr, [](vkStructureChain*) {}});
		}
		chain.last()->pNext = nullptr;
	};

	// chain needs to be of size 2+ for the pnext logic to work
	if (chain.size() == 0) {
		clone();
		ptr = ptr->pNext;
	}

	for (; ptr != nullptr; ptr = ptr->pNext) {
		clone();
		chain[chain.size() - 2]->pNext = chain.last().get();
	}
}

namespace device::featureStruct {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"

#include "vkm/inc/reflect_struct_deviceFeatureStruct.inc"  // IWYU pragma: keep

#pragma GCC diagnostic pop
}  // namespace device::featureStruct
}  // namespace vkm::vk::reflect
