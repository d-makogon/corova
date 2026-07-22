#include <libloom/coroutines.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "test_support.h"

typedef struct {
  int fd;
  EventType event;
  unsigned started;
  unsigned resumed;
  bool wait_satisfied;
} Waiter;

static void wait_for_event(void *arg) {
  Waiter *waiter = arg;
  waiter->started++;
  waiter->wait_satisfied = coroutine_wait_fd(waiter->fd, waiter->event);
  waiter->resumed++;
}

static void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  CHECK(flags != -1);
  CHECK(fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
}

static void fill_pipe(int fd) {
  char buffer[PIPE_BUF] = {0};
  for (;;) {
    ssize_t written = write(fd, buffer, sizeof(buffer));
    if (written >= 0)
      continue;
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    return;
  }
}

int main(void) {
  int readable_pipe[2];
  int writable_pipe[2];
  Waiter reader;
  Waiter writer;
  char byte = 'x';
  char writable_space[PIPE_BUF];

  CHECK(pipe(readable_pipe) == 0);
  CHECK(pipe(writable_pipe) == 0);
  set_nonblocking(writable_pipe[1]);
  fill_pipe(writable_pipe[1]);

  reader = (Waiter){.fd = readable_pipe[0], .event = CORO_WAIT_READ};
  writer = (Waiter){.fd = writable_pipe[1], .event = CORO_WAIT_WRITE};

  coroutine_init();
  coroutine_go(wait_for_event, &reader);
  coroutine_go(wait_for_event, &writer);

  // Both waiters start and suspend. The empty/full pipes cannot satisfy either
  // request, so neither coroutine may be scheduled past coroutine_wait_fd().
  coroutine_yield();
  CHECK(reader.started == 1);
  CHECK(writer.started == 1);
  CHECK(reader.resumed == 0);
  CHECK(writer.resumed == 0);
  CHECK(coroutines_alive() == 3);

  CHECK(write(readable_pipe[1], &byte, sizeof(byte)) == (ssize_t)sizeof(byte));
  CHECK(read(writable_pipe[0], writable_space, sizeof(writable_space)) ==
        (ssize_t)sizeof(writable_space));

  coroutines_run();

  CHECK(reader.wait_satisfied);
  CHECK(writer.wait_satisfied);
  CHECK(reader.resumed == 1);
  CHECK(writer.resumed == 1);

  CHECK(close(readable_pipe[0]) == 0);
  CHECK(close(readable_pipe[1]) == 0);
  CHECK(close(writable_pipe[0]) == 0);
  CHECK(close(writable_pipe[1]) == 0);
  return EXIT_SUCCESS;
}
