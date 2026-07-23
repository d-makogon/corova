cmake_minimum_required(VERSION 3.10)
project(CorovaCoroutinesLib VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CLIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/clib")

if(NOT EXISTS "${CLIB_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "The clib submodule is missing. Run: git submodule update --init --recursive")
endif()

add_subdirectory("${CLIB_SOURCE_DIR}")

set(LIB_SRC
  src/coroutines.c
)

set(LIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(LIB_INCLUDE
  ${LIB_INCLUDE_DIR}/libloom/coroutines.h
)

set(SHARED_LIB_NAME libloom_shared)
set(STATIC_LIB_NAME libloom_static)

add_library(${SHARED_LIB_NAME} SHARED
  ${LIB_SRC}
  ${LIB_INCLUDE}
)

set_target_properties(${SHARED_LIB_NAME} PROPERTIES OUTPUT_NAME "loom")

add_library(${STATIC_LIB_NAME} STATIC
  ${LIB_SRC}
  ${LIB_INCLUDE}
)

set_target_properties(${STATIC_LIB_NAME} PROPERTIES OUTPUT_NAME "loom")

# 2. Define include directories
# PUBLIC means anything linking to this library also gets these headers
target_include_directories(${SHARED_LIB_NAME} PUBLIC
  $<BUILD_INTERFACE:${LIB_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)

target_include_directories(${STATIC_LIB_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(${SHARED_LIB_NAME} PRIVATE clib_shared)
target_link_libraries(${STATIC_LIB_NAME} PRIVATE clib_static)

set(COMPILER_OPTIONS
  -Wall
  -Wextra
)

target_compile_options(${SHARED_LIB_NAME} PRIVATE ${COMPILER_OPTIONS})
target_compile_options(${STATIC_LIB_NAME} PRIVATE ${COMPILER_OPTIONS})

if(ENABLE_LOGS)
  target_compile_definitions(${SHARED_LIB_NAME} PRIVATE ENABLE_LOG)
  target_compile_definitions(${STATIC_LIB_NAME} PRIVATE ENABLE_LOG)
endif()

install(TARGETS ${STATIC_LIB_NAME} DESTINATION lib)
install(TARGETS ${SHARED_LIB_NAME} DESTINATION lib)

install(
  DIRECTORY "${LIB_INCLUDE_DIR}/"
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.h"
)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

add_subdirectory(examples)
