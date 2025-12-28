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

#include <stdio.h>
#include <stdlib.h>

namespace vkm::std {
struct sourceLocation {
	const char* const func;
	const char* const file;
	const int line;

	static consteval sourceLocation current(const char* func = __builtin_FUNCTION(),
											const char* file = __builtin_FILE(), int line = __builtin_LINE()) noexcept {
		return sourceLocation{func, file, line};
	}
};

inline static void abort(const char* msg = nullptr, sourceLocation loc = sourceLocation::current()) {
	static constexpr const char* locFmt = "Fatal Error At: %s %s:%d\n%s";
	fprintf(stderr, locFmt, loc.func, loc.file, loc.line, msg);
	::abort();
}

template <typename T>
inline static void debugRun([[maybe_unused]] T fn) {
#ifndef NDEBUG
	fn();
#endif
}
}  // namespace vkm::std
