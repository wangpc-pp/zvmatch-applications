cmake_minimum_required(VERSION 3.20)

project(zvmatch_applications LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ZVMATCH_APPLICATIONS_STATIC_LINK
       "Link the RISC-V test executables statically" ON)

set_source_files_properties(
  run.c
  PROPERTIES COMPILE_OPTIONS "-fno-vectorize;-fno-slp-vectorize")

function(add_application_runner target implementation march)
  add_executable(${target} run.c keysets.c ${implementation})
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_options(
    ${target}
    PRIVATE
      -O2
      -march=${march}
      -menable-experimental-extensions
      -Wall
      -Wextra
      -Werror)
  target_link_options(
    ${target}
    PRIVATE
      -march=${march}
      -menable-experimental-extensions)

  if(ZVMATCH_APPLICATIONS_STATIC_LINK)
    target_link_options(${target} PRIVATE -static)
  endif()
endfunction()

add_application_runner(run-scalar reference.c rv64gc)
add_application_runner(run-rvv rvv_without_zvmatch.c rv64gcv)
add_application_runner(run-zvmatch rvv_with_zvmatch.c rv64gcv_zvmatch0p1)
add_application_runner(run-zvmatch-vx rvv_with_zvmatch_vx.c rv64gcv_zvmatch0p1)

add_custom_target(
  run
  COMMAND
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-qemu.sh"
    "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS run-scalar run-rvv run-zvmatch run-zvmatch-vx
  USES_TERMINAL)

add_custom_target(
  instruction-count
  COMMAND
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/count-instructions.sh"
    "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS run-scalar run-rvv run-zvmatch run-zvmatch-vx
  USES_TERMINAL)
