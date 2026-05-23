add_executable(counters
    counters.c
)

target_link_libraries(counters PRIVATE ${SHARED_LIB_NAME})

target_include_directories(counters PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(counters PRIVATE
    -Wall
    -Wextra
)
