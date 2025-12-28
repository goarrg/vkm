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

#include <stdint.h>

namespace vkm::std::unit {
namespace memory {
using size = uint64_t;

[[maybe_unused]] static constexpr size byte = 1;

[[maybe_unused]] static constexpr size kilobyte = 1000 * byte;
[[maybe_unused]] static constexpr size megabyte = 1000 * kilobyte;
[[maybe_unused]] static constexpr size gigabyte = 1000 * megabyte;
[[maybe_unused]] static constexpr size terabyte = 1000 * megabyte;
[[maybe_unused]] static constexpr size petabyte = 1000 * terabyte;

[[maybe_unused]] static constexpr size kibibyte = 1024 * byte;
[[maybe_unused]] static constexpr size mebibyte = 1024 * kibibyte;
[[maybe_unused]] static constexpr size gibibyte = 1024 * mebibyte;
[[maybe_unused]] static constexpr size tebibyte = 1024 * gibibyte;
[[maybe_unused]] static constexpr size pebibyte = 1024 * tebibyte;
}  // namespace memory
}  // namespace vkm::std::unit
