#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <clib/dyn_array.h>

static const unsigned COROUTINE_STACK_SIZE = 1024 * 1024;

static unsigned cur_coroutine_index = 0;

typedef struct {
  void *sp;
} CoroutineContext;

typedef struct {
  DYN_ARRAY_FIELDS(CoroutineContext);
} Contexts;

static Contexts contexts = {0};

void coroutine_init(void) {
  CoroutineContext ctx = {0};
  da_append(&contexts, ctx);
}

void coroutine_fail_on_normal_return(void) {
  fprintf(stderr, "Coroutine %u returned. It is unimplemented\n",
          cur_coroutine_index);
  abort();
}

void coroutine_go(void (*job)(void)) {
  assert(job);
  void *stack = aligned_alloc(16, COROUTINE_STACK_SIZE);
  stack = (unsigned char *)stack + COROUTINE_STACK_SIZE;
  assert(stack);
  void **stack_p = stack;
  *(--stack_p) = 0;                                // Unused.
  *(--stack_p) = &coroutine_fail_on_normal_return; // Initial LR.
  *(--stack_p) = job; // Branch destination. On coroutine entry it's its job
                      // function address.
  *(--stack_p) = 0;   // No initial frame.
  *(--stack_p) = 0;
  *(--stack_p) = 0;
  *(--stack_p) = 0;
  *(--stack_p) = 0;
  *(--stack_p) = 0;
  *(--stack_p) = 0;
  *(--stack_p) = 0;
  *(--stack_p) = 0;
  *(--stack_p) = 0;
  *(--stack_p) = 0;

  CoroutineContext ctx = {0};
  ctx.sp = stack_p;
  da_append(&contexts, ctx);
  printf("Coroutine #%zu added, sp is %p, saved fp is %p\n",
         da_count(&contexts), ctx.sp, NULL);
}

__attribute__((naked)) void coroutine_yield() {
  __asm__ volatile(
      "str x30, [sp, #-16]!\n" // Save x30. Note that here it is kind of spare.
      "stp x29, x30, [sp, #-16]!\n" // Save FP and LR (the address to branch to
                                    // after waking up).
      "stp x19, x20, [sp, #-16]!\n" // Callee saved registers
      "stp x21, x22, [sp, #-16]!\n"
      "stp x23, x24, [sp, #-16]!\n"
      "stp x25, x26, [sp, #-16]!\n"
      "stp x27, x28, [sp, #-16]!\n"
      "mov x0, sp\n"
      "bl _coroutine_yield_impl");
  // TODO: save d8, d9, d10, d11, d12, d13, d14, d15, d16, d17, d18, d19,
  //            d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31.
}

__attribute__((naked)) void coroutine_restore_ctx(void *sp) {
  __asm__ volatile("mov sp, x0\n"
                   "ldp x27, x28, [sp], #16\n" // Callee saved registers
                   "ldp x25, x26, [sp], #16\n"
                   "ldp x23, x24, [sp], #16\n"
                   "ldp x21, x22, [sp], #16\n"
                   "ldp x19, x20, [sp], #16\n"
                   "ldp x29, x1, [sp], #16\n" // FP, LR
                   "ldr x30, [sp], #16\n"
                   "br x1");
}

void coroutine_yield_impl(void *sp) {
  CoroutineContext *cur_ctx = &contexts.items[cur_coroutine_index];
  cur_ctx->sp = sp;
  printf("Yielding coroutine #%u\n", cur_coroutine_index);
  unsigned next_coroutine = (cur_coroutine_index + 1) % da_count(&contexts);
  CoroutineContext *next_coroutine_ctx = &contexts.items[next_coroutine];
  assert(next_coroutine_ctx);
  cur_coroutine_index = next_coroutine;
  coroutine_restore_ctx(next_coroutine_ctx->sp);
}

void counter(void) {
  for (unsigned i = 10; i-- > 0;) {
    printf("%u\n", i);
    coroutine_yield();
  }
}

int main(int argc, char **argv) {
  coroutine_init();
  coroutine_go(counter);
  coroutine_go(counter);
  while (1) {
    coroutine_yield();
  }

  return 0;
}
