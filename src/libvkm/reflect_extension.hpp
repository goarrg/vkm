/*
Copyright 2026 The goARRG Authors.

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

#include "vkm/vkm.h"  // IWYU pragma: keep

#include <stdint.h>
#include <string.h>

#include "vkm/std/array.hpp"   // IWYU pragma: keep
#include "vkm/std/vector.hpp"  // IWYU pragma: keep
#include "vkm/std/string.hpp"  // IWYU pragma: keep

namespace vkm::vk::reflect {
struct extensionInfo {
	enum kind : uint8_t { instance, device };
	const char* name;
	kind kind;
	uint32_t promotion;

	constexpr extensionInfo(const char* name, enum kind kind, uint32_t promotion) noexcept
		: name(name), kind(kind), promotion(promotion) {}
	virtual constexpr ~extensionInfo() noexcept = default;

	[[nodiscard]] constexpr bool isCoreAt(uint32_t version) const { return promotion != 0 && promotion < version; }

	[[nodiscard]] virtual constexpr size_t numInstanceDependencies() const noexcept = 0;
	[[nodiscard]] virtual constexpr const char* instanceDependency(size_t i) const noexcept = 0;

	[[nodiscard]] virtual constexpr size_t numDeviceDependencies() const noexcept = 0;
	[[nodiscard]] virtual constexpr const char* deviceDependency(size_t i) const noexcept = 0;
};
namespace internal {
template <size_t I, size_t D>
struct extensionInfoImpl : extensionInfo {
   private:
	const vkm::std::array<const char*, I> instanceDependencies;
	const vkm::std::array<const char*, D> deviceDependencies;

   public:
	constexpr extensionInfoImpl(const char* name, enum ::vkm::vk::reflect::extensionInfo::kind kind, uint32_t promotion,
								vkm::std::array<const char*, I>&& instanceDependencies,
								vkm::std::array<const char*, D>&& deviceDependencies) noexcept
		: extensionInfo(name, kind, promotion),
		  instanceDependencies(vkm::std::move(instanceDependencies)),
		  deviceDependencies(vkm::std::move(deviceDependencies)) {}
	constexpr ~extensionInfoImpl() noexcept override = default;

	[[nodiscard]] constexpr size_t numInstanceDependencies() const noexcept override { return I; }
	[[nodiscard]] constexpr const char* instanceDependency(size_t i) const noexcept override {
		return instanceDependencies[i];
	}

	[[nodiscard]] constexpr size_t numDeviceDependencies() const noexcept override { return D; }
	[[nodiscard]] constexpr const char* deviceDependency(size_t i) const noexcept override {
		return deviceDependencies[i];
	}
};
}  // namespace internal

#include "vkm/inc/reflect_extension.inc"

}  // namespace vkm::vk::reflect
