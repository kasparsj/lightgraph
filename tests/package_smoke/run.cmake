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

file(REMOVE_RECURSE "${binary_dir}" "${install_dir}")

execute_process(
  COMMAND "${cmake_command}" --install "${main_build_dir}" --prefix "${install_dir}"
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
    "-DCMAKE_PREFIX_PATH=${install_dir}"
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Package smoke configure failed with code ${configure_result}")
endif()

execute_process(
  COMMAND "${cmake_command}" --build "${binary_dir}" --parallel
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Package smoke build failed with code ${build_result}")
endif()

if(WIN32)
  set(smoke_exe "${binary_dir}/lightgraph_package_smoke.exe")
else()
  set(smoke_exe "${binary_dir}/lightgraph_package_smoke")
endif()

execute_process(
  COMMAND "${smoke_exe}"
  RESULT_VARIABLE run_result
)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "Package smoke executable failed with code ${run_result}")
endif()
