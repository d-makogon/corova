#ifndef COROVA_COROUTINES_H
#define COROVA_COROUTINES_H

void coroutine_init(void);
void coroutine_go(void (*job)(void *), void *arg);
void coroutine_yield(void);
void coroutine_finish(void);
unsigned coroutine_id(void);
unsigned coroutines_alive(void);

#endif // COROVA_COROUTINES_H
