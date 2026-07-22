function(add_coroutine_test name)
  add_executable(${name} ${name}.c)
  target_link_libraries(${name} PRIVATE ${STATIC_LIB_NAME})
  target_compile_options(${name} PRIVATE ${COMPILER_OPTIONS})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_coroutine_test(coroutine_scheduling_test)
add_coroutine_test(coroutine_wait_fd_test)
