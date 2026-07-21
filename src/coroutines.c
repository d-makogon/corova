#include <corova/coroutines.h>

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <clib/dyn_array.h>
#include <clib/hash_map.h>
#include <clib/linked_list.h>

#ifdef ENABLE_LOG
#define LOG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define LOG(...)
#endif

static const unsigned COROUTINE_STACK_SIZE = 4 * 1024;

static unsigned cur_context_id = 0;
static unsigned next_coroutine_id = 0;

typedef struct {
  int fd;
  EventType events_mask;
  bool satisfied;
} WaitInfo;

typedef struct {
  unsigned id;
  void *stack_base;
  void *sp;
  WaitInfo wait_info;
} CoroutineContext;

typedef struct {
  unsigned key; // The context ID
  CoroutineContext value;
} ContextIDPair;

static unsigned coro_hash(const void *p) { return *(const unsigned *)p; }
static bool coro_cmp(const void *p1, const void *p2) {
  return *(const unsigned *)p1 == *(const unsigned *)p2;
}

typedef struct {
  HASH_MAP_FIELDS(ContextIDPair)
} Contexts;

typedef struct CoroIDListNode {
  LINKED_LIST_NODE_FIELDS(unsigned, struct CoroIDListNode);
} CoroIDListNode;

typedef struct {
  LINKED_LIST_FIELDS(CoroIDListNode);
} CoroIDsList;

typedef CoroIDsList CoroutineIDsList;

typedef struct {
  DYN_ARRAY_FIELDS(unsigned)
} CoroutineIDs;

static Contexts contexts = {0};
static CoroIDsList ready_coro_queue = {0};
static CoroutineIDs waiting_coroutines = {0};

static void add_context(CoroutineContext *ctx) {
  hm_put(&contexts, (ContextIDPair){.key = ctx->id, .value = *ctx});
}

static CoroutineContext *get_context(unsigned id) {
  CoroutineContext *ctx = hm_get(&contexts, id);
  assert(ctx && "Must have context");
  return ctx;
}

static CoroutineContext *cur_context(void) {
  return get_context(cur_context_id);
}

static void push_ready_coroutine(unsigned id) {
  list_append(&ready_coro_queue, id);
}

static unsigned pop_ready_coroutine() {
  assert(!list_empty(&ready_coro_queue) && "Must have available coroutines");
  unsigned id = ready_coro_queue.head->value;
  list_pop_front(&ready_coro_queue);
  return id;
}

static void poll_waiting_coroutines(void) {
  assert(cur_context()->id == 0 &&
         "Must only be called from the main coroutine");

  typedef struct {
    DYN_ARRAY_FIELDS(struct pollfd);
  } PollFDs;

  PollFDs poll_fds = {0};
  da_reserve(&poll_fds, waiting_coroutines.count);
  da_foreach(&waiting_coroutines, coro_id) {
    CoroutineContext *coroutine = get_context(*coro_id);
    assert(!coroutine->wait_info.satisfied &&
           "Request must not be satisfied yet");
    struct pollfd pfd = {0};
    pfd.fd = coroutine->wait_info.fd;
    if (coroutine->wait_info.events_mask & CORO_WAIT_READ)
      pfd.events = POLLIN;
    if (coroutine->wait_info.events_mask & CORO_WAIT_WRITE)
      pfd.events = POLLOUT;
    LOG("Adding coroutine #%u to wait on fd %d for events %u\n", coroutine->id,
        pfd.fd, pfd.events);
    da_append(&poll_fds, pfd);
  }

  int timeout = 0;
  if (list_empty(&ready_coro_queue)) {
    LOG("Blocking indefinitely until there are ready coroutines...\n");
    timeout = -1;
  }

  int ret = poll(poll_fds.items, poll_fds.count, timeout);
  if (ret == -1) {
    perror("poll failed");
    abort();
  }

  unsigned removed_num = 0;
  da_enumerate(&poll_fds, index, pollfd) {
    unsigned waiting_index = index - removed_num;
    unsigned coro_id = waiting_coroutines.items[waiting_index];
    CoroutineContext *coroutine = get_context(coro_id);
    assert(coroutine->wait_info.fd == pollfd->fd);
    if ((pollfd->events & POLLIN && pollfd->revents & POLLIN) ||
        (pollfd->events & POLLOUT && pollfd->revents & POLLOUT)) {
      LOG("Coroutine #%u has received events %d, making it ready\n",
          coroutine->id, pollfd->revents);
      coroutine->wait_info.satisfied = true;
      push_ready_coroutine(coroutine->id);
      da_remove(&waiting_coroutines, waiting_index);
      removed_num++;
    }
  }
}

void coroutine_init(void) {
  hm_init(&contexts, coro_hash, coro_cmp);
  CoroutineContext ctx = {0};
  assert(next_coroutine_id == 0 &&
         "Must be called before adding any coroutines");
  add_context(&ctx);
  next_coroutine_id++;
}

static void switch_to_next_ready(void);

void coroutine_finish(void) {
  CoroutineContext *ctx = cur_context();
  if (ctx->id == 0) {
    hm_free(&contexts);
    list_free(&ready_coro_queue);
    return;
  }

  free(ctx->stack_base);
  hm_remove(&contexts, ctx->id);
  LOG("Coroutine %u finished.\n", ctx->id);
  switch_to_next_ready();
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

  add_context(&ctx);
  push_ready_coroutine(ctx.id);
  LOG("Coroutine #%zu added, sp is %p, stack base is %p\n", da_count(&contexts),
      ctx.sp, stack_base);
}

__attribute__((naked)) void coroutine_yield_asm(bool is_ready) {
  asm volatile(
      "mov x1, x0\n"
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
      "bl _save_ctx_and_switch_to_next_ready");
  // TODO: save d8, d9, d10, d11, d12, d13, d14, d15, d16, d17, d18, d19,
  //            d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31.
}

void coroutine_yield(void) { coroutine_yield_asm(/*is_ready=*/true); }

void coroutine_yield_not_ready(void) {
  coroutine_yield_asm(/*is_ready=*/false);
}

__attribute__((naked)) static void coroutine_restore_ctx(void *sp) {
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

static void coroutine_switch_ctx(unsigned coroutine_id) {
  LOG("Switching to coroutine #%u\n", coroutine_id);
  CoroutineContext *next_coroutine_ctx = get_context(coroutine_id);
  cur_context_id = next_coroutine_ctx->id;
  coroutine_restore_ctx(next_coroutine_ctx->sp);
}

static void switch_to_next_ready(void) {
  coroutine_switch_ctx(pop_ready_coroutine());
}

__attribute__((used)) static void
save_ctx_and_switch_to_next_ready(void *sp, bool is_ready) {
  CoroutineContext *cur_ctx = cur_context();
  cur_ctx->sp = sp;
  LOG("Yielding coroutine #%u, is ready = %d\n", cur_ctx->id, is_ready);
  if (is_ready)
    push_ready_coroutine(cur_ctx->id);
  switch_to_next_ready();
}

bool coroutine_wait_fd(int fd, EventType events_mask) {
  CoroutineContext *coro = cur_context();
  coro->wait_info =
      (WaitInfo){.fd = fd, .events_mask = events_mask, .satisfied = false};
  da_append(&waiting_coroutines, coro->id);
  LOG("Coroutine #%u waits for fd %d, events = %d\n", coro->id, fd,
      events_mask);
  coroutine_yield_not_ready();
  bool satisfied = coro->wait_info.satisfied;
  coro->wait_info = (WaitInfo){0};
  LOG("Coroutine #%u request satisfied: %d for fd %d, events = %d\n", satisfied,
      coro->id, fd, events_mask);
  return satisfied;
}

unsigned coroutine_id(void) { return cur_context()->id; }

unsigned coroutines_alive(void) { return contexts.count; }

void coroutines_run(void) {
  assert(cur_context()->id == 0 && "Must be called from the main coroutine");
  while (coroutines_alive() > 1) {
    poll_waiting_coroutines();
    coroutine_yield();
  }
  coroutine_finish();
}
