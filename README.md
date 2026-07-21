# Corova

Corova is a small cooperative stackful coroutine library for C.

Coroutines run on one thread and switch only when they yield, wait for an fd,
or return.

## Platform Support

Corova currently supports **Apple arm64 only**.

This is an experimental implementation. Coroutine stacks are currently fixed
at 4 KiB.

## Requirements

- macOS on Apple silicon
- CMake 3.10 or newer
- A C23 compiler, such as Clang
- The `clib` library built locally
- Ninja or another CMake-supported build tool

## Build

Build [clib](https://github.com/d-makogon/clib) first:

```sh
cmake -S /path/to/clib -B /path/to/clib/build -G Ninja
cmake --build /path/to/clib/build
```

Then configure Corova with the `clib` library and include directories:

```sh
cmake -S . -B build -G Ninja \
  -DCLIB_LIBRARY_PATH=/path/to/clib/build \
  -DCLIB_INCLUDE_PATH=/path/to/clib/include
cmake --build build
```

This produces:

- `build/libcorova.a`: static library
- `build/libcorova.dylib`: shared library
- `build/examples/*`: examples

Run the basic example with:

```sh
./build/examples/counters
```

## Basic Usage

Include the public header, initialize the scheduler, add jobs, and run until all
jobs return:

```c
#include <stdbool.h>
#include <stdio.h>
#include <corova/coroutines.h>

static void counter(void *argument) {
  unsigned limit = *(const unsigned *)argument;

  for (unsigned i = 0; i < limit; ++i) {
    printf("coroutine %u: %u\n", coroutine_id(), i);
    coroutine_yield();
  }
}

int main(void) {
  unsigned first_limit = 3;
  unsigned second_limit = 5;

  coroutine_init();
  coroutine_go(counter, &first_limit);
  coroutine_go(counter, &second_limit);
  coroutines_run();
  return 0;
}
```

Returning from a job finishes the job's coroutine.

For nonblocking I/O, suspend the current coroutine until an fd is ready:

```c
coroutine_wait_fd(fd, CORO_WAIT_READ);
coroutine_wait_fd(fd, CORO_WAIT_WRITE);
```

## Use In Another CMake Project

Build `clib` first, set its paths, and add Corova as a subdirectory:

```cmake
cmake_minimum_required(VERSION 3.10)
project(example LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CLIB_LIBRARY_PATH "/absolute/path/to/clib/build")
set(CLIB_INCLUDE_PATH "/absolute/path/to/clib/include")

add_subdirectory(/absolute/path/to/coroutines corova)

add_executable(example main.c)
target_link_libraries(example PRIVATE corova_static)
```

Use `corova_shared` instead of `corova_static` for dynamic linking. Linking the
CMake target adds the Corova and `clib` include directories transitively.

## Use A Built Library Directly

Compile against both Corova and `clib`:

```sh
cc -std=gnu2x main.c \
  -I/path/to/coroutines/include \
  -I/path/to/clib/include \
  /path/to/coroutines/build/libcorova.a \
  /path/to/clib/build/libclib.a \
  -o example
```
