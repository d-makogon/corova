#ifndef COROVA_COROUTINES_H
#define COROVA_COROUTINES_H

void coroutine_init(void);
void coroutine_go(void (*job)(void *), void *arg);
void coroutine_yield(void);

typedef enum { CORO_WAIT_READ = 0x1, CORO_WAIT_WRITE = 0x2 } EventType;

bool coroutine_wait_fd(int fd, EventType events_mask);

void coroutine_finish(void);

unsigned coroutine_id(void);
unsigned coroutines_alive(void);

void coroutines_run(void);
#endif // COROVA_COROUTINES_H
