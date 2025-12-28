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

namespace vkm::std {
template <typename T>
class defer {
   private:
	T fn;

   public:
	defer() = delete;
	defer(const defer&) = delete;
	defer& operator=(const defer&) = delete;

	constexpr defer(T fn) noexcept : fn(fn) {}
	~defer() noexcept { fn(); }
};
}  // namespace vkm::std

#define DEFER_CONCAT2(a, b) a##b
#define DEFER_CONCAT(a, b) DEFER_CONCAT2(a, b)

#define DEFER(body) auto DEFER_CONCAT(_defer, __LINE__) = ::vkm::std::defer(body)
