cmake_minimum_required(VERSION 3.20)

project(lightpath_core LANGUAGES CXX)

option(LIGHTPATH_CORE_BUILD_TESTS "Build Lightpath core tests" ON)
option(LIGHTPATH_CORE_BUILD_EXAMPLES "Build Lightpath examples" ON)
option(LIGHTPATH_CORE_ENABLE_ASAN "Enable AddressSanitizer for host builds" OFF)
option(LIGHTPATH_CORE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for host builds" OFF)
option(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings and treat warnings as errors" OFF)
option(
  LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS
  "Expose src/ as a public include root for transitional internal module includes"
  OFF
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(
  GLOB_RECURSE LIGHTPATH_CORE_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src/*.cpp"
)

add_library(lightpath_core STATIC ${LIGHTPATH_CORE_SOURCES})
add_library(lightpath::lightpath ALIAS lightpath_core)

target_include_directories(
  lightpath_core
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS)
  target_include_directories(lightpath_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(
      LIGHTPATH_CORE_STRICT_WARNING_FLAGS
      -Wall
      -Wextra
      -Wpedantic
      -Werror
    )
    target_compile_options(
      lightpath_core
      PRIVATE
        ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS}
    )

    set(
      LIGHTPATH_CORE_VENDOR_WARNING_SUPPRESSIONS
      -Wno-delete-non-abstract-non-virtual-dtor
      -Wno-overloaded-virtual
      -Wno-reorder-ctor
      -Wno-sometimes-uninitialized
      -Wno-undefined-var-template
    )
    set_source_files_properties(
      "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/ColorTheory.cpp"
      PROPERTIES
        COMPILE_OPTIONS "${LIGHTPATH_CORE_VENDOR_WARNING_SUPPRESSIONS}"
    )
  elseif(MSVC)
    set(LIGHTPATH_CORE_STRICT_WARNING_FLAGS /W4 /WX)
    target_compile_options(lightpath_core PRIVATE /W4 /WX)
  else()
    message(WARNING "LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS is not configured for this compiler")
  endif()
endif()

if(LIGHTPATH_CORE_ENABLE_ASAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightpath_core PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(lightpath_core PUBLIC -fsanitize=address)
  else()
    message(WARNING "LIGHTPATH_CORE_ENABLE_ASAN is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTPATH_CORE_ENABLE_UBSAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightpath_core PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined)
    target_link_options(lightpath_core PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined)
  else()
    message(WARNING "LIGHTPATH_CORE_ENABLE_UBSAN is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTPATH_CORE_BUILD_EXAMPLES)
  add_executable(
    lightpath_core_minimal_example
    examples/minimal_usage.cpp
  )
  target_link_libraries(lightpath_core_minimal_example PRIVATE lightpath_core)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_minimal_example PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()
endif()

if(LIGHTPATH_CORE_BUILD_TESTS)
  enable_testing()

  if(TARGET lightpath_core_minimal_example)
    add_test(NAME lightpath_core_example COMMAND lightpath_core_minimal_example)
  endif()

  add_executable(
    lightpath_core_smoke
    tests/core_smoke_test.cpp
  )

  target_link_libraries(lightpath_core_smoke PRIVATE lightpath_core)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_smoke PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightpath_core_smoke COMMAND lightpath_core_smoke)

  add_executable(
    lightpath_core_regression
    tests/core_regression_test.cpp
  )

  target_link_libraries(lightpath_core_regression PRIVATE lightpath_core)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_regression PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightpath_core_regression COMMAND lightpath_core_regression)

  add_executable(
    lightpath_core_public_api
    tests/public_api_test.cpp
  )

  target_link_libraries(lightpath_core_public_api PRIVATE lightpath_core)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_public_api PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightpath_core_public_api COMMAND lightpath_core_public_api)
endif()
