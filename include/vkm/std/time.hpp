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

namespace vkm::std::time {
using duration = uint64_t;
[[maybe_unused]] static constexpr duration nanosecond = 1;
[[maybe_unused]] static constexpr duration microsecond = 1000 * nanosecond;
[[maybe_unused]] static constexpr duration millisecond = 1000 * microsecond;
[[maybe_unused]] static constexpr duration second = 1000 * millisecond;
[[maybe_unused]] static constexpr duration minute = 60 * second;
[[maybe_unused]] static constexpr duration hour = 60 * minute;
}  // namespace vkm::std::time
