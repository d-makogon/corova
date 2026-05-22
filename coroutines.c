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
  void *fp;
  void *pc;
  void *lr;
  uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
  // TODO: double d8, d9, d10, d11, d12, d13, d14, d15, d16, d17, d18, d19, d20,
  //              d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31;
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
  fprintf(stderr,
          "Coroutine %u executed normal return, which is prohibited for "
          "coroutines. To finish a coroutine, use coroutine_finish",
          cur_coroutine_index);
  abort();
}

void coroutine_finish(void) {
  assert(false && "Coroutine finish is not implemented yet");
}

void coroutine_go(void (*job)(void)) {
  assert(job);
  unsigned char *stack = aligned_alloc(16, COROUTINE_STACK_SIZE);
  assert(stack);
  CoroutineContext ctx = {0};
  ctx.sp = stack + COROUTINE_STACK_SIZE;
  ctx.fp = 0;
  ctx.lr = &coroutine_fail_on_normal_return;
  ctx.pc = job;
  da_append(&contexts, ctx);
  printf("Coroutine #%zu added\n", da_count(&contexts));
}

__attribute__((naked)) void coroutine_yield() {
  __asm__ volatile("stp x29, x30, [sp, #-16]!\n"
                   "mov x1, x29\n" // fp
                   "mov x29, sp\n"
                   "add x0, sp, #16\n" // sp
                   "mov x2, x30\n"     // lr
                   "mov x3, x19\n"     // callee saved registers
                   "mov x4, x20\n"
                   "mov x5, x21\n"
                   "mov x6, x22\n"
                   "mov x7, x23\n"
                   "mov x8, x24\n"
                   "mov x9, x25\n"
                   "mov x10, x26\n"
                   "mov x11, x27\n"
                   "mov x12, x28\n"
                   "bl _coroutine_yield_impl\n"
                   "ldp x29, x30, [sp], #16\n"
                   "ret");
}

__attribute__((naked)) void coroutine_restore_ctx(CoroutineContext *ctx) {
  __asm__ volatile("ldr x6, [x0, #0]\n"   // sp
                   "ldr x5, [x0, #8]\n"   // fp
                   "ldr x8, [x0, #16]\n"  // pc
                   "ldr x3, [x0, #24]\n"  // lr
                   "ldr x19, [x0, #32]\n" // callee saved registers
                   "ldr x20, [x0, #40]\n"
                   "ldr x21, [x0, #48]\n"
                   "ldr x22, [x0, #56]\n"
                   "ldr x23, [x0, #64]\n"
                   "ldr x24, [x0, #72]\n"
                   "ldr x25, [x0, #80]\n"
                   "ldr x26, [x0, #88]\n"
                   "ldr x27, [x0, #96]\n"
                   "ldr x28, [x0, #104]\n"
                   "mov sp, x6\n"
                   "mov x29, x5\n"
                   "mov x30, x3\n"
                   "br x8");
}

void coroutine_yield_impl(void *sp, void *fp, void *lr, uint64_t x19,
                          uint64_t x20, uint64_t x21, uint64_t x22,
                          uint64_t x23, uint64_t x24, uint64_t x25,
                          uint64_t x26, uint64_t x27, uint64_t x28) {
  CoroutineContext *cur_ctx = &contexts.items[cur_coroutine_index];
  cur_ctx->sp = sp;
  cur_ctx->fp = fp;
  cur_ctx->pc = lr;
  cur_ctx->x19 = x19;
  cur_ctx->x20 = x20;
  cur_ctx->x20 = x20;
  cur_ctx->x21 = x21;
  cur_ctx->x22 = x22;
  cur_ctx->x23 = x23;
  cur_ctx->x24 = x24;
  cur_ctx->x25 = x25;
  cur_ctx->x26 = x26;
  cur_ctx->x27 = x27;
  cur_ctx->x28 = x28;
  printf("Yielding coroutine #%u\n", cur_coroutine_index);
  unsigned next_coroutine = (cur_coroutine_index + 1) % da_count(&contexts);
  CoroutineContext *next_coroutine_ctx = &contexts.items[next_coroutine];
  cur_coroutine_index = next_coroutine;
  coroutine_restore_ctx(next_coroutine_ctx);
}

void counter(void) {
  for (unsigned i = 10; i-- > 0;) {
    printf("%u\n", i);
    coroutine_yield();
  }
  coroutine_finish();
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
