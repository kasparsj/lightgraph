cmake_minimum_required(VERSION 3.20)

project(lightgraph VERSION 1.0.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(LIGHTGRAPH_CORE_BUILD_TESTS "Build Lightgraph core tests" ON)
option(LIGHTGRAPH_CORE_BUILD_EXAMPLES "Build Lightgraph core examples" ON)
option(LIGHTGRAPH_CORE_BUILD_BENCHMARKS "Build Lightgraph micro-benchmarks" OFF)
option(LIGHTGRAPH_CORE_BUILD_DOCS "Build Lightgraph Doxygen docs target" OFF)
option(LIGHTGRAPH_CORE_ENABLE_ASAN "Enable AddressSanitizer for host builds" OFF)
option(LIGHTGRAPH_CORE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for host builds" OFF)
option(LIGHTGRAPH_CORE_ENABLE_COVERAGE "Enable gcov/llvm-cov coverage instrumentation" OFF)
option(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings and treat warnings as errors" OFF)
option(
  LIGHTGRAPH_CORE_ENABLE_LEGACY_INCLUDE_PATHS
  "Expose include/src as public include roots for transitional source integrations"
  OFF
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIGHTGRAPH_CORE_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
set(LIGHTGRAPH_CORE_STABLE_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/stable/include")
file(MAKE_DIRECTORY "${LIGHTGRAPH_CORE_GENERATED_INCLUDE_DIR}/lightgraph")
file(MAKE_DIRECTORY "${LIGHTGRAPH_CORE_STABLE_INCLUDE_DIR}/lightgraph")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in"
  "${LIGHTGRAPH_CORE_GENERATED_INCLUDE_DIR}/lightgraph/version.hpp"
  @ONLY
)

set(
  LIGHTGRAPH_CORE_STABLE_HEADER_NAMES
  lightgraph.hpp
  engine.hpp
  status.hpp
  types.hpp
)

foreach(header_name IN LISTS LIGHTGRAPH_CORE_STABLE_HEADER_NAMES)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lightgraph/${header_name}"
    "${LIGHTGRAPH_CORE_STABLE_INCLUDE_DIR}/lightgraph/${header_name}"
    COPYONLY
  )
endforeach()

set(
  LIGHTGRAPH_CORE_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/FastNoise.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/Globals.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/Random.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/Engine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/debug/Debugger.cpp"
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
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/RuntimeLight.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Light.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/LightList.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/State.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Connection.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Intersection.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/TopologyObject.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Owner.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Model.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Port.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/topology/Weight.cpp"
)

set(
  LIGHTGRAPH_CORE_VENDOR_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/ColorTheory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src/ColorUtil.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src/ColorWheelScheme.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src/ColorWheelSchemes.cpp"
)

add_library(lightgraph_vendor_colortheory OBJECT ${LIGHTGRAPH_CORE_VENDOR_SOURCES})
target_include_directories(
  lightgraph_vendor_colortheory
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ofxColorTheory/src"
)

add_library(lightgraph STATIC ${LIGHTGRAPH_CORE_SOURCES} $<TARGET_OBJECTS:lightgraph_vendor_colortheory>)
add_library(lightgraph::lightgraph ALIAS lightgraph)
# Source-tree compatibility alias.
add_library(lightgraph_core ALIAS lightgraph)
set_target_properties(lightgraph PROPERTIES EXPORT_NAME lightgraph)

target_include_directories(
  lightgraph
  PUBLIC
    "$<BUILD_INTERFACE:${LIGHTGRAPH_CORE_STABLE_INCLUDE_DIR}>"
    "$<BUILD_INTERFACE:${LIGHTGRAPH_CORE_GENERATED_INCLUDE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/host/include"
)

# Source-integration headers are intentionally not part of the installed API.
add_library(lightgraph_integration INTERFACE)
add_library(lightgraph::integration ALIAS lightgraph_integration)
target_link_libraries(lightgraph_integration INTERFACE lightgraph)
target_include_directories(
  lightgraph_integration
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)

if(LIGHTGRAPH_CORE_ENABLE_LEGACY_INCLUDE_PATHS)
  target_include_directories(
    lightgraph
    PUBLIC
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
endif()

set(
  LIGHTGRAPH_CORE_PUBLIC_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lightgraph/lightgraph.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lightgraph/engine.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lightgraph/status.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lightgraph/types.hpp"
)

set(
  LIGHTGRAPH_CORE_GENERATED_PUBLIC_HEADERS
  "${LIGHTGRAPH_CORE_GENERATED_INCLUDE_DIR}/lightgraph/version.hpp"
)

install(
  TARGETS lightgraph
  EXPORT lightgraphTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
  FILES ${LIGHTGRAPH_CORE_PUBLIC_HEADERS}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/lightgraph"
)

install(
  FILES ${LIGHTGRAPH_CORE_GENERATED_PUBLIC_HEADERS}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/lightgraph"
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lightgraphConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lightgraphConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/lightgraph"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/lightgraphConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)

install(
  EXPORT lightgraphTargets
  FILE lightgraphTargets.cmake
  NAMESPACE lightgraph::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/lightgraph"
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/lightgraphConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/lightgraphConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/lightgraph"
)

if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(
      LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS
      -Wall
      -Wextra
      -Wpedantic
      -Werror
    )
    target_compile_options(
      lightgraph
      PRIVATE
        ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS}
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
      LIGHTGRAPH_CORE_VENDOR_WARNING_SUPPRESSIONS
      -Wno-delete-non-abstract-non-virtual-dtor
      -Wno-overloaded-virtual
      -Wno-reorder-ctor
      -Wno-sometimes-uninitialized
      -Wno-undefined-var-template
    )
    target_compile_options(
      lightgraph_vendor_colortheory
      PRIVATE
        ${LIGHTGRAPH_CORE_VENDOR_WARNING_SUPPRESSIONS}
    )
  elseif(MSVC)
    set(LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS /W4 /WX)
    target_compile_options(lightgraph PRIVATE /W4 /WX)
  else()
    message(WARNING "LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS is not configured for this compiler")
  endif()
endif()

if(LIGHTGRAPH_CORE_ENABLE_ASAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightgraph PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    target_compile_options(lightgraph_vendor_colortheory PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(lightgraph PUBLIC -fsanitize=address)
  else()
    message(WARNING "LIGHTGRAPH_CORE_ENABLE_ASAN is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTGRAPH_CORE_ENABLE_UBSAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightgraph PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined)
    target_compile_options(lightgraph_vendor_colortheory PRIVATE -fsanitize=undefined -fno-sanitize-recover=undefined)
    target_link_options(lightgraph PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined)
  else()
    message(WARNING "LIGHTGRAPH_CORE_ENABLE_UBSAN is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTGRAPH_CORE_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightgraph PUBLIC --coverage -O0 -g)
    target_compile_options(lightgraph_vendor_colortheory PRIVATE --coverage -O0 -g)
    target_link_options(lightgraph PUBLIC --coverage)
  else()
    message(WARNING "LIGHTGRAPH_CORE_ENABLE_COVERAGE is only supported with Clang/GCC")
  endif()
endif()

if(LIGHTGRAPH_CORE_BUILD_EXAMPLES)
  add_executable(
    lightgraph_core_minimal_example
    examples/minimal_usage.cpp
  )
  target_link_libraries(lightgraph_core_minimal_example PRIVATE lightgraph)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_minimal_example PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_executable(
    lightgraph_core_integration_example
    examples/integration_host_loop.cpp
  )
  target_link_libraries(lightgraph_core_integration_example PRIVATE lightgraph::integration)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_integration_example PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()
endif()

if(LIGHTGRAPH_CORE_BUILD_BENCHMARKS)
  add_executable(
    lightgraph_core_benchmark
    benchmarks/core_benchmark.cpp
  )
  target_link_libraries(lightgraph_core_benchmark PRIVATE lightgraph)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_benchmark PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()
endif()

if(LIGHTGRAPH_CORE_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  add_custom_target(
    lightgraph_core_docs
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/build/docs/html"
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating Lightgraph API documentation"
    VERBATIM
  )
endif()

if(LIGHTGRAPH_CORE_BUILD_TESTS)
  enable_testing()

  if(TARGET lightgraph_core_minimal_example)
    add_test(NAME lightgraph_core_example COMMAND lightgraph_core_minimal_example)
  endif()

  if(TARGET lightgraph_core_integration_example)
    add_test(NAME lightgraph_core_integration_example COMMAND lightgraph_core_integration_example)
  endif()

  add_executable(
    lightgraph_core_smoke
    tests/core_smoke_test.cpp
  )

  target_link_libraries(lightgraph_core_smoke PRIVATE lightgraph)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_smoke PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightgraph_core_smoke COMMAND lightgraph_core_smoke)

  add_executable(
    lightgraph_core_regression
    tests/core_regression_test.cpp
  )

  target_link_libraries(lightgraph_core_regression PRIVATE lightgraph)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_regression PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightgraph_core_regression COMMAND lightgraph_core_regression)

  add_executable(
    lightgraph_core_public_api
    tests/public_api_test.cpp
  )

  target_link_libraries(lightgraph_core_public_api PRIVATE lightgraph)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_public_api PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()

  add_test(NAME lightgraph_core_public_api COMMAND lightgraph_core_public_api)

  add_executable(
    lightgraph_core_api_fuzz
    tests/api_fuzz_test.cpp
  )
  target_link_libraries(lightgraph_core_api_fuzz PRIVATE lightgraph)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_api_fuzz PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()
  add_test(NAME lightgraph_core_api_fuzz COMMAND lightgraph_core_api_fuzz)

  add_executable(
    lightgraph_core_mutation_edges
    tests/core_mutation_edge_test.cpp
  )
  target_link_libraries(lightgraph_core_mutation_edges PRIVATE lightgraph)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_mutation_edges PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()
  add_test(NAME lightgraph_core_mutation_edges COMMAND lightgraph_core_mutation_edges)

  add_executable(
    lightgraph_core_topology_emit
    tests/core_topology_emit_test.cpp
  )
  target_link_libraries(lightgraph_core_topology_emit PRIVATE lightgraph)
  if(LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS)
    target_compile_options(lightgraph_core_topology_emit PRIVATE ${LIGHTGRAPH_CORE_STRICT_WARNING_FLAGS})
  endif()
  add_test(NAME lightgraph_core_topology_emit COMMAND lightgraph_core_topology_emit)

  add_test(
    NAME lightgraph_core_package_smoke
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
