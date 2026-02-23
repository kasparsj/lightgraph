cmake_minimum_required(VERSION 3.20)

project(lightpath_core VERSION 1.0.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(LIGHTPATH_CORE_BUILD_TESTS "Build Lightpath core tests" ON)
option(LIGHTPATH_CORE_BUILD_EXAMPLES "Build Lightpath examples" ON)
option(LIGHTPATH_CORE_BUILD_BENCHMARKS "Build Lightpath micro-benchmarks" OFF)
option(LIGHTPATH_CORE_BUILD_DOCS "Build Lightpath Doxygen docs target" OFF)
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

set(LIGHTPATH_CORE_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${LIGHTPATH_CORE_GENERATED_INCLUDE_DIR}/lightpath")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in"
  "${LIGHTPATH_CORE_GENERATED_INCLUDE_DIR}/lightpath/version.hpp"
  @ONLY
)

file(
  GLOB_RECURSE LIGHTPATH_CORE_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src/*.cpp"
)

add_library(lightpath_core STATIC ${LIGHTPATH_CORE_SOURCES})
add_library(lightpath::lightpath ALIAS lightpath_core)
set_target_properties(lightpath_core PROPERTIES EXPORT_NAME lightpath)

target_include_directories(
  lightpath_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${LIGHTPATH_CORE_GENERATED_INCLUDE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
)

if(LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS)
  target_include_directories(lightpath_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

set(
  LIGHTPATH_CORE_PUBLIC_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lightpath/lightpath.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lightpath/engine.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lightpath/status.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lightpath/types.hpp"
)

set(
  LIGHTPATH_CORE_GENERATED_PUBLIC_HEADERS
  "${LIGHTPATH_CORE_GENERATED_INCLUDE_DIR}/lightpath/version.hpp"
)

install(
  TARGETS lightpath_core
  EXPORT lightpathTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
  FILES ${LIGHTPATH_CORE_PUBLIC_HEADERS}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/lightpath"
)

install(
  FILES ${LIGHTPATH_CORE_GENERATED_PUBLIC_HEADERS}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/lightpath"
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lightpathConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lightpathConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/lightpath"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/lightpathConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)

install(
  EXPORT lightpathTargets
  FILE lightpathTargets.cmake
  NAMESPACE lightpath::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/lightpath"
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/lightpathConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/lightpathConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/lightpath"
)

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

if(LIGHTPATH_CORE_BUILD_BENCHMARKS)
  add_executable(
    lightpath_core_benchmark
    benchmarks/core_benchmark.cpp
  )
  target_link_libraries(lightpath_core_benchmark PRIVATE lightpath_core)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_benchmark PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()
endif()

if(LIGHTPATH_CORE_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  add_custom_target(
    lightpath_core_docs
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating Lightpath API documentation"
    VERBATIM
  )
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

  add_executable(
    lightpath_core_api_fuzz
    tests/api_fuzz_test.cpp
  )
  target_link_libraries(lightpath_core_api_fuzz PRIVATE lightpath_core)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_api_fuzz PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()
  add_test(NAME lightpath_core_api_fuzz COMMAND lightpath_core_api_fuzz)

  add_executable(
    lightpath_core_mutation_edges
    tests/core_mutation_edge_test.cpp
  )
  target_link_libraries(lightpath_core_mutation_edges PRIVATE lightpath_core)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_mutation_edges PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()
  add_test(NAME lightpath_core_mutation_edges COMMAND lightpath_core_mutation_edges)

  add_test(
    NAME lightpath_core_package_smoke
    COMMAND
      ${CMAKE_COMMAND}
        -Dcmake_command=${CMAKE_COMMAND}
        -Dmain_build_dir=${CMAKE_BINARY_DIR}
        -Dsource_dir=${CMAKE_CURRENT_SOURCE_DIR}/tests/package_smoke
        -Dbinary_dir=${CMAKE_BINARY_DIR}/tests/package_smoke/build
        -Dinstall_dir=${CMAKE_BINARY_DIR}/tests/package_smoke/install
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/package_smoke/run.cmake
  )
endif()
