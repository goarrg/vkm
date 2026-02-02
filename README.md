# vkm - vulkan managed
vkm is a C API bootstrap library that doesn't end at device creation.
It aims to selectively manage parts of vulkan, simplifying app development
in the most common areas.

The design philosophy is "UX first but only compromise performance/flexibility
if such a user is unlikely to use vkm or that such a design would be too opinionated/cumbersome to be broadly useful"

## Status
vkm is in a request for comments mode while we figure out the tooling and other work unrelated to the API.

## Features
- Minimal use of C++ stdlib/STL
  - vector/array/string/etc... has been NIHed
  - Entire list of headers is `new, type_traits, concepts, algorithm`
    - Only reason algorithm is there is for sorting, could be removed eventually
- Automatic vulkan extension dependencies injection
- Automatic extension deduction from feature struct for non promoted structs
  - If the struct is associated with multiple extensions you are required to manually pass which one do you wish to use, as we have no way of knowing your requirements
- No differentiation of instance/device extension when creating both through vkm
- Built in function pointer loader with a list trimmed to the lowest vulkan version vkm works with (eventually this may be selectable)

### Managed objects
- vkInstance creation/destruction
- vkDevice creation/destruction
- Multiple vkQueues creation
  - We do not have API for managed inter queue sync yet but you can provide your own semaphores
- Swapchain management
  - You are still required to signal acquire/present/resize but it is much nice to work with

## Requirements
- go 1.24 toolchain as the build system uses go
- C++20 compiler (gcc/clang)

## Limitations
- No build system intergration other than pkg-config
- No MSVC support (yet), if you have ideas, file a issue
- Does not handle OS specific or provisional feature structs (we do not know how to support this)
- Could use more docs

## Install
- Create a new folder and cd to it
- Run `go mod init example.com`
  - This initializes the folder as a go project, example.com is a url that can be used to fetch your project
- Run `go get goarrg.com/lib/vkm/cmd/make`
  - This downloads vkm and configures your project to depend on it
- Run `go run goarrg.com/lib/vkm/cmd/make`
  - This actually builds vkm, the results will be in `.goarrg/cgodep/vkm/*`
  - We have a pkg-config replacement at `.goarrg/tool/cgodep-config` this is able to query goarrg built libs with a fallback to the system's pkg-config for external libs
