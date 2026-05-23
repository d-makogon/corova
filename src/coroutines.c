#include <corova/coroutines.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <clib/dyn_array.h>

#ifdef ENABLE_LOG
#define LOG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define LOG(...)
#endif

static const unsigned COROUTINE_STACK_SIZE = 4 * 1024;

static unsigned cur_context_index = 0;
static unsigned next_coroutine_id = 0;

typedef struct {
  unsigned id;
  void *stack_base;
  void *sp;
} CoroutineContext;

typedef struct {
  DYN_ARRAY_FIELDS(CoroutineContext);
} Contexts;

static Contexts contexts = {0};

void coroutine_init(void) {
  CoroutineContext ctx = {0};
  assert(next_coroutine_id == 0);
  next_coroutine_id++;
  da_append(&contexts, ctx);
}
void coroutine_switch_ctx(unsigned coroutine_idx);

void coroutine_finish(void) {
  // Remove the context.
  CoroutineContext *ctx = &contexts.items[cur_context_index];
  free((unsigned char *)ctx->stack_base);
  // Move the last context into the place of the removed one.
  contexts.items[cur_context_index] = contexts.items[contexts.count - 1];
  contexts.count--;
  LOG("Coroutine %u returned, context.count = %zu.\n", cur_coroutine_index,
      contexts.count);
  coroutine_switch_ctx((cur_context_index + 1) % contexts.count);
}

void coroutine_go(void (*job)(void *), void *arg) {
  assert(job);
  void *stack_base = aligned_alloc(16, COROUTINE_STACK_SIZE);
  assert(stack_base);
  void *stack = (unsigned char *)stack_base + COROUTINE_STACK_SIZE;
  void **stack_p = stack;
  *(--stack_p) = arg;               // Argument to the coroutine.
  *(--stack_p) = &coroutine_finish; // LR. Set it to our return interceptor to
                                    // free up the coroutine context.
  *(--stack_p) = job; // Branch destination. On coroutine entry it's the job
                      // function address.
  *(--stack_p) = 0;   // FP (no initial frame).
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
  ctx.id = next_coroutine_id++;
  ctx.stack_base = stack_base;
  ctx.sp = stack_p;
  da_append(&contexts, ctx);
  LOG("Coroutine #%zu added, sp is %p, stack base is %p\n", da_count(&contexts),
      ctx.sp, stack_base);
}

__attribute__((naked)) void coroutine_yield() {
  asm volatile(
      "stp x30, x0, [sp, #-16]!\n"  // Save x30. Note that here it is kind of
                                    // spare as we also save it below, but this
                                    // slot is needed to store the final return
                                    // address before the coroutine first entry.
                                    // Also save the coroutine argument.
      "stp x29, x30, [sp, #-16]!\n" // Save FP and LR (the address to branch to
                                    // after waking up).
      "stp x19, x20, [sp, #-16]!\n" // Callee saved registers.
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
  asm volatile("mov sp, x0\n"
               "ldp x27, x28, [sp], #16\n" // Callee saved registers.
               "ldp x25, x26, [sp], #16\n"
               "ldp x23, x24, [sp], #16\n"
               "ldp x21, x22, [sp], #16\n"
               "ldp x19, x20, [sp], #16\n"
               "ldp x29, x1, [sp], #16\n" // FP and the address to branch to.
               "ldp x30, x0, [sp], #16\n" // LR and the coroutine argument.
               "br x1");
}

void coroutine_switch_ctx(unsigned coroutine_idx) {
  LOG("Switching to coroutine #%u\n", coroutine_idx);
  CoroutineContext *next_coroutine_ctx = &contexts.items[coroutine_idx];
  assert(next_coroutine_ctx);
  cur_context_index = coroutine_idx;
  coroutine_restore_ctx(next_coroutine_ctx->sp);
}

void coroutine_yield_impl(void *sp) {
  CoroutineContext *cur_ctx = &contexts.items[cur_context_index];
  cur_ctx->sp = sp;
  LOG("Yielding coroutine #%u\n", cur_coroutine_index);
  unsigned next_coroutine = (cur_context_index + 1) % da_count(&contexts);
  coroutine_switch_ctx(next_coroutine);
}

unsigned coroutine_id(void) { return contexts.items[cur_context_index].id; }

unsigned coroutines_alive(void) { return contexts.count; }
