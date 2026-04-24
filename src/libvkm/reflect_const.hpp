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

#include "vkm/std/utility.hpp"	// IWYU pragma: keep
#include "vkm/std/string.hpp"	// IWYU pragma: keep

namespace vkm::vk::reflect {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"

#include "vkm/inc/reflect_const.inc"

#pragma GCC diagnostic pop
}  // namespace vkm::vk::reflect
