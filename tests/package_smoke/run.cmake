if(NOT DEFINED cmake_command)
  message(FATAL_ERROR "cmake_command is required")
endif()

if(NOT DEFINED main_build_dir)
  message(FATAL_ERROR "main_build_dir is required")
endif()

if(NOT DEFINED source_dir)
  message(FATAL_ERROR "source_dir is required")
endif()

if(NOT DEFINED binary_dir)
  message(FATAL_ERROR "binary_dir is required")
endif()

if(NOT DEFINED install_dir)
  message(FATAL_ERROR "install_dir is required")
endif()

set(run_cwd "${CMAKE_CURRENT_BINARY_DIR}")

macro(resolve_to_absolute path_var)
  if(NOT IS_ABSOLUTE "${${path_var}}")
    cmake_path(ABSOLUTE_PATH ${path_var} BASE_DIRECTORY "${run_cwd}" NORMALIZE)
  else()
    cmake_path(NORMAL_PATH ${path_var})
  endif()
endmacro()

resolve_to_absolute(main_build_dir)
resolve_to_absolute(source_dir)
resolve_to_absolute(binary_dir)
resolve_to_absolute(install_dir)

# Direct callers may supply only paths; recover the toolchain from the parent.
load_cache("${main_build_dir}" READ_WITH_PREFIX main_
  CMAKE_GENERATOR CMAKE_CXX_COMPILER CMAKE_BUILD_TYPE CMAKE_CONFIGURATION_TYPES)
if(NOT DEFINED generator)
  set(generator "${main_CMAKE_GENERATOR}")
endif()
if(NOT DEFINED cxx_compiler)
  set(cxx_compiler "${main_CMAKE_CXX_COMPILER}")
endif()
if(NOT DEFINED multi_config)
  set(multi_config FALSE)
  if(main_CMAKE_CONFIGURATION_TYPES)
    set(multi_config TRUE)
  endif()
endif()
if(NOT DEFINED build_config)
  set(build_config "${main_CMAKE_BUILD_TYPE}")
  if(multi_config)
    set(build_config Release)
  endif()
endif()

file(REMOVE_RECURSE "${binary_dir}" "${install_dir}")

execute_process(
  COMMAND "${cmake_command}" --install "${main_build_dir}"
    --config "${build_config}" --prefix "${install_dir}"
  RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "Install step failed with code ${install_result}")
endif()

execute_process(
  COMMAND
    "${cmake_command}"
    -S "${source_dir}"
    -B "${binary_dir}"
    -G "${generator}"
    "-DCMAKE_CXX_COMPILER=${cxx_compiler}"
    "-DCMAKE_BUILD_TYPE=${build_config}"
    "-DCMAKE_PREFIX_PATH=${install_dir}"
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Package smoke configure failed with code ${configure_result}")
endif()

execute_process(
  COMMAND "${cmake_command}" --build "${binary_dir}"
    --config "${build_config}" --parallel
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Package smoke build failed with code ${build_result}")
endif()

set(smoke_exe_dir "${binary_dir}")
if(multi_config)
  string(APPEND smoke_exe_dir "/${build_config}")
endif()
set(smoke_exe "${smoke_exe_dir}/lightgraph_package_smoke")
if(WIN32)
  string(APPEND smoke_exe ".exe")
endif()

execute_process(
  COMMAND "${smoke_exe}"
  RESULT_VARIABLE run_result
)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "Package smoke executable failed with code ${run_result}")
endif()
