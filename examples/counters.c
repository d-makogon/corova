#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <corova/coroutines.h>

void counter(void *arg) {
  for (unsigned i = (int)arg; i-- > 0;) {
    printf("[%u] %u\n", coroutine_id(), i);
    coroutine_yield();
  }
}

int main(int argc, char **argv) {
  coroutine_init();
  coroutine_go(counter, (void *)5);
  coroutine_go(counter, (void *)10);
  while (coroutines_alive() > 1) {
    coroutine_yield();
  }
  coroutine_go(counter, (void *)5);
  coroutine_go(counter, (void *)10);
  while (coroutines_alive() > 1) {
    coroutine_yield();
  }

  return 0;
}
