# libloom

libloom is a small cooperative stackful coroutine library for C.

Coroutines run on one thread and switch only when they yield, wait for an fd,
or return.

## Platform Support

libloom currently supports **Apple arm64 only**.

This is an experimental implementation. Coroutine stacks are currently fixed
at 4 KiB.

## Requirements

- macOS on Apple silicon
- CMake 3.10 or newer
- A C23 compiler, such as Clang
- Git (to initialize the `clib` submodule)
- Ninja or another CMake-supported build tool

## Build

Clone libloom with its submodules:

```sh
git clone --recurse-submodules https://github.com/d-makogon/corova.git libloom
cd libloom
```

Then configure and build libloom:

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

This produces:

- `build/libloom.a`: static library
- `build/libloom.dylib`: shared library
- `build/examples/*`: examples

## Testing

Run the test suite with:

```sh
ninja test
```

or

```sh
ctest --test-dir path/to/build --output-on-failure
```

## Basic Usage

Include the public header, initialize the scheduler, add jobs, and run until all
jobs return:

```c
#include <stdbool.h>
#include <stdio.h>
#include <libloom/coroutines.h>

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

See basic usage examples in [examples](examples).

Run them with:

```sh
./build/examples/counters
```

## Use In Another CMake Project

Initialize libloom's submodules, then add it as a subdirectory:

```cmake
cmake_minimum_required(VERSION 3.10)
project(example LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(/absolute/path/to/coroutines libloom)

add_executable(example main.c)
target_link_libraries(example PRIVATE libloom_static)
```

Use `libloom_shared` instead of `libloom_static` for dynamic linking. Linking the
CMake target provides the libraries required by libloom.

## Use A Built Library Directly

Compile against both `libloom` and the bundled `clib`:

```sh
cc -std=gnu2x main.c \
  -I/path/to/coroutines/include \
  /path/to/coroutines/build/libloom.a \
  /path/to/coroutines/build/external/clib/libclib.a \
  -o example
```
