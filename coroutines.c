#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static const unsigned COROUTINES_CAPACITY = 10;
static const unsigned COROUTINE_STACK_SIZE = 1024 * 1024;
static const unsigned TOTAL_STACK_MEMORY =
    COROUTINE_STACK_SIZE * COROUTINES_CAPACITY;

static unsigned coroutines_count = 0;
static unsigned cur_coroutine_index = 0;
static unsigned char *stacks_begin = NULL;

typedef struct {
  void *sp;
  void *fp;
  void *pc;
  void *lr;
} CoroutineContext;

static CoroutineContext *contexts = NULL;

void coroutine_init(void) {
  assert(coroutines_count + 1 < COROUTINES_CAPACITY);
  ++coroutines_count;
}

void coroutine_ret(void) {
  assert(false && "Coroutine must not execute this code");
}

void coroutine_go(void (*job)(void)) {
  assert(job);
  assert(coroutines_count + 1 < COROUTINES_CAPACITY);
  unsigned char *stack =
      stacks_begin + (coroutines_count * COROUTINE_STACK_SIZE);
  contexts[coroutines_count].sp = stack;
  contexts[coroutines_count].fp = stack;
  contexts[coroutines_count].lr = &coroutine_ret;
  contexts[coroutines_count].pc = job;
  printf("Coroutine #%d added\n", coroutines_count);
  coroutines_count++;
}

__attribute__((naked)) void coroutine_yield() {
  __asm__("stp x29, x30, [sp, #-16]!\n"
          "mov x1, x29\n" // fp
          "mov x29, sp\n"
          "add x0, sp, #16\n" // sp
          "mov x2, x30\n"     // lr
          "bl _coroutine_yield_impl\n"
          "ldp x29, x30, [sp], #16\n"
          "ret");
}

// TODO: can be non-naked
__attribute__((naked)) void coroutine_restore_ctx(CoroutineContext *ctx) {
  __asm__("ldr x6, [x0, #0]\n"  // sp
          "ldr x5, [x0, #8]\n"  // fp
          "ldr x8, [x0, #16]\n" // pc
          "ldr x3, [x0, #24]\n" // lr
          "mov sp, x6\n"
          "mov x29, x5\n"
          "mov x30, x3\n"
          "br x8");
}

void coroutine_yield_impl(void *sp, void *fp, void *lr) {
  contexts[cur_coroutine_index].sp = sp;
  contexts[cur_coroutine_index].fp = fp;
  contexts[cur_coroutine_index].pc = lr;
  printf("Yielding coroutine #%u\n", cur_coroutine_index);
  unsigned next_coroutine = (cur_coroutine_index + 1) % coroutines_count;
  CoroutineContext *next_coroutine_ctx = &contexts[next_coroutine];
  cur_coroutine_index = next_coroutine;
  coroutine_restore_ctx(next_coroutine_ctx);
}

void counter(void) {
  for (unsigned i = 10; i-- > 0;) {
    printf("%u\n", i);
    coroutine_yield();
  }
}

int main(int argc, char **argv) {
  stacks_begin = malloc(TOTAL_STACK_MEMORY * sizeof(*stacks_begin));
  assert(stacks_begin);
  contexts = calloc(COROUTINES_CAPACITY, sizeof(*contexts));
  assert(contexts);

  coroutine_init();
  coroutine_go(counter);
  coroutine_go(counter);
  while (1) {
    coroutine_yield();
  }

  return 0;
}
