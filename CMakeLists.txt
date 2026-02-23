cmake_minimum_required(VERSION 3.20)

project(lightpath VERSION 1.0.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(LIGHTPATH_CORE_BUILD_TESTS "Build Lightpath core tests" ON)
option(LIGHTPATH_CORE_BUILD_EXAMPLES "Build Lightpath core examples" ON)
option(LIGHTPATH_CORE_BUILD_BENCHMARKS "Build Lightpath micro-benchmarks" OFF)
option(LIGHTPATH_CORE_BUILD_DOCS "Build Lightpath Doxygen docs target" OFF)
option(LIGHTPATH_CORE_ENABLE_ASAN "Enable AddressSanitizer for host builds" OFF)
option(LIGHTPATH_CORE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for host builds" OFF)
option(LIGHTPATH_CORE_ENABLE_COVERAGE "Enable gcov/llvm-cov coverage instrumentation" OFF)
option(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings and treat warnings as errors" OFF)
option(
  LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS
  "Expose include/src as public include roots for transitional source integrations"
  OFF
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIGHTPATH_CORE_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
set(LIGHTPATH_CORE_STABLE_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/stable/include")
file(MAKE_DIRECTORY "${LIGHTPATH_CORE_GENERATED_INCLUDE_DIR}/lightpath")
file(MAKE_DIRECTORY "${LIGHTPATH_CORE_STABLE_INCLUDE_DIR}/lightpath")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in"
  "${LIGHTPATH_CORE_GENERATED_INCLUDE_DIR}/lightpath/version.hpp"
  @ONLY
)

set(
  LIGHTPATH_CORE_STABLE_HEADER_NAMES
  lightpath.hpp
  engine.hpp
  status.hpp
  types.hpp
)

foreach(header_name IN LISTS LIGHTPATH_CORE_STABLE_HEADER_NAMES)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lightpath/${header_name}"
    "${LIGHTPATH_CORE_STABLE_INCLUDE_DIR}/lightpath/${header_name}"
    COPYONLY
  )
endforeach()

set(
  LIGHTPATH_CORE_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/FastNoise.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/Globals.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/LPRandom.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/Engine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/debug/LPDebugger.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/objects/Cross.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/objects/Heptagon3024.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/objects/Heptagon919.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/objects/HeptagonStar.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/objects/Line.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/objects/Triangle.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/Palette.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/Palettes.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Behaviour.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/BgLight.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/EmitParams.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/LPLight.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Light.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/LightList.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/State.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Connection.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Intersection.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/LPObject.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/LPOwner.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Model.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Port.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Weight.cpp"
)

set(
  LIGHTPATH_CORE_VENDOR_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/ColorTheory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src/ColorUtil.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src/ColorWheelScheme.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src/ColorWheelSchemes.cpp"
)

add_library(lightpath_vendor_colortheory OBJECT ${LIGHTPATH_CORE_VENDOR_SOURCES})
target_include_directories(
  lightpath_vendor_colortheory
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src"
)

add_library(lightpath STATIC ${LIGHTPATH_CORE_SOURCES} $<TARGET_OBJECTS:lightpath_vendor_colortheory>)
add_library(lightpath::lightpath ALIAS lightpath)
# Source-tree compatibility alias.
add_library(lightpath_core ALIAS lightpath)
set_target_properties(lightpath PROPERTIES EXPORT_NAME lightpath)

target_include_directories(
  lightpath
  PUBLIC
    "$<BUILD_INTERFACE:${LIGHTPATH_CORE_STABLE_INCLUDE_DIR}>"
    "$<BUILD_INTERFACE:${LIGHTPATH_CORE_GENERATED_INCLUDE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
)

# Source-integration headers are intentionally not part of the installed API.
add_library(lightpath_integration INTERFACE)
add_library(lightpath::integration ALIAS lightpath_integration)
target_link_libraries(lightpath_integration INTERFACE lightpath)
target_include_directories(
  lightpath_integration
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)

if(LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS)
  target_include_directories(
    lightpath
    PUBLIC
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
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
  TARGETS lightpath
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
      lightpath
      PRIVATE
        ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS}
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      # FastNoise is vendored legacy code; keep strict mode while carving out
      # the known GCC-only diagnostics that are non-trivial to rewrite.
      set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/FastNoise.cpp"
        PROPERTIES
          COMPILE_OPTIONS
            "-Wno-error=strict-aliasing;-Wno-error=implicit-fallthrough;-Wno-error=maybe-uninitialized;-Wno-strict-aliasing"
      )
    endif()

    set(
      LIGHTPATH_CORE_VENDOR_WARNING_SUPPRESSIONS
      -Wno-delete-non-abstract-non-virtual-dtor
      -Wno-overloaded-virtual
      -Wno-reorder-ctor
      -Wno-sometimes-uninitialized
      -Wno-undefined-var-template
    )
    target_compile_options(
      lightpath_vendor_colortheory
      PRIVATE
        ${LIGHTPATH_CORE_VENDOR_WARNING_SUPPRESSIONS}
    )
  elseif(MSVC)
    set(LIGHTPATH_CORE_STRICT_WARNING_FLAGS /W4 /WX)
    target_compile_options(lightpath PRIVATE /W4 /WX)
  else()
    message(WARNING "LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS is not configured for this compiler")
  endif()
endif()

if(LIGHTPATH_CORE_ENABLE_ASAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightpath PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    target_compile_options(lightpath_vendor_colortheory PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(lightpath PUBLIC -fsanitize=address)
  else()
    message(WARNING "LIGHTPATH_CORE_ENABLE_ASAN is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTPATH_CORE_ENABLE_UBSAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightpath PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined)
    target_compile_options(lightpath_vendor_colortheory PRIVATE -fsanitize=undefined -fno-sanitize-recover=undefined)
    target_link_options(lightpath PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined)
  else()
    message(WARNING "LIGHTPATH_CORE_ENABLE_UBSAN is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTPATH_CORE_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightpath PUBLIC --coverage -O0 -g)
    target_compile_options(lightpath_vendor_colortheory PRIVATE --coverage -O0 -g)
    target_link_options(lightpath PUBLIC --coverage)
  else()
    message(WARNING "LIGHTPATH_CORE_ENABLE_COVERAGE is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTPATH_CORE_BUILD_EXAMPLES)
  add_executable(
    lightpath_core_minimal_example
    examples/minimal_usage.cpp
  )
  target_link_libraries(lightpath_core_minimal_example PRIVATE lightpath)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_minimal_example PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_executable(
    lightpath_core_integration_example
    examples/integration_host_loop.cpp
  )
  target_link_libraries(lightpath_core_integration_example PRIVATE lightpath::integration)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_integration_example PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()
endif()

if(LIGHTPATH_CORE_BUILD_BENCHMARKS)
  add_executable(
    lightpath_core_benchmark
    benchmarks/core_benchmark.cpp
  )
  target_link_libraries(lightpath_core_benchmark PRIVATE lightpath)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_benchmark PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()
endif()

if(LIGHTPATH_CORE_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  add_custom_target(
    lightpath_core_docs
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/build/docs/html"
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

  if(TARGET lightpath_core_integration_example)
    add_test(NAME lightpath_core_integration_example COMMAND lightpath_core_integration_example)
  endif()

  add_executable(
    lightpath_core_smoke
    tests/core_smoke_test.cpp
  )

  target_link_libraries(lightpath_core_smoke PRIVATE lightpath)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_smoke PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightpath_core_smoke COMMAND lightpath_core_smoke)

  add_executable(
    lightpath_core_regression
    tests/core_regression_test.cpp
  )

  target_link_libraries(lightpath_core_regression PRIVATE lightpath)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_regression PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightpath_core_regression COMMAND lightpath_core_regression)

  add_executable(
    lightpath_core_public_api
    tests/public_api_test.cpp
  )

  target_link_libraries(lightpath_core_public_api PRIVATE lightpath)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_public_api PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightpath_core_public_api COMMAND lightpath_core_public_api)

  add_executable(
    lightpath_core_api_fuzz
    tests/api_fuzz_test.cpp
  )
  target_link_libraries(lightpath_core_api_fuzz PRIVATE lightpath)
  if(LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightpath_core_api_fuzz PRIVATE ${LIGHTPATH_CORE_STRICT_WARNING_FLAGS})
  endif()
  add_test(NAME lightpath_core_api_fuzz COMMAND lightpath_core_api_fuzz)

  add_executable(
    lightpath_core_mutation_edges
    tests/core_mutation_edge_test.cpp
  )
  target_link_libraries(lightpath_core_mutation_edges PRIVATE lightpath)
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
