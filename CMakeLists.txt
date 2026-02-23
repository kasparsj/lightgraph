cmake_minimum_required(VERSION 3.20)

project(lightgraph_core LANGUAGES CXX)

option(LIGHTGRAPH_CORE_BUILD_TESTS "Build Lightgraph core tests" ON)
option(LIGHTGRAPH_CORE_ENABLE_ASAN "Enable AddressSanitizer for host builds" OFF)
option(LIGHTGRAPH_CORE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for host builds" OFF)
option(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings and treat warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(
  GLOB_RECURSE LIGHTGRAPH_CORE_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src/*.cpp"
)

add_library(lightgraph_core STATIC ${LIGHTGRAPH_CORE_SOURCES})

target_include_directories(
  lightgraph_core
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(
      LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS
      -Wall
      -Wextra
      -Wpedantic
      -Werror
      -Wno-unsequenced
      -Wno-delete-non-abstract-non-virtual-dtor
      -Wno-overloaded-virtual
      -Wno-reorder-ctor
      -Wno-sometimes-uninitialized
      -Wno-undefined-var-template
    )
    target_compile_options(
      lightgraph_core
      PRIVATE
        ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS}
    )
  elseif(MSVC)
    set(LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS /W4 /WX)
    target_compile_options(lightgraph_core PRIVATE /W4 /WX)
  else()
    message(WARNING "LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS is not configured for this compiler")
  endif()
endif()

if(LIGHTGRAPH_CORE_ENABLE_ASAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightgraph_core PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(lightgraph_core PUBLIC -fsanitize=address)
  else()
    message(WARNING "LIGHTGRAPH_CORE_ENABLE_ASAN is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTGRAPH_CORE_ENABLE_UBSAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightgraph_core PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined)
    target_link_options(lightgraph_core PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined)
  else()
    message(WARNING "LIGHTGRAPH_CORE_ENABLE_UBSAN is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTGRAPH_CORE_BUILD_TESTS)
  enable_testing()

  add_executable(
    lightgraph_core_smoke
    tests/core_smoke_test.cpp
  )

  target_link_libraries(lightgraph_core_smoke PRIVATE lightgraph_core)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_smoke PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightgraph_core_smoke COMMAND lightgraph_core_smoke)

  add_executable(
    lightgraph_core_regression
    tests/core_regression_test.cpp
  )

  target_link_libraries(lightgraph_core_regression PRIVATE lightgraph_core)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_regression PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightgraph_core_regression COMMAND lightgraph_core_regression)
endif()
