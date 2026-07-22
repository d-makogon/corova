#include <libloom/coroutines.h>

#include <stddef.h>

#include "test_support.h"

enum { MAX_EVENTS = 16 };

typedef struct {
  char name;
  unsigned turns;
  unsigned id;
  unsigned runs;
} Counter;

static char events[MAX_EVENTS];
static size_t event_count;

static void record(char event) {
  CHECK(event_count < MAX_EVENTS);
  events[event_count++] = event;
}

static void counter(void *arg) {
  Counter *state = arg;
  state->id = coroutine_id();

  for (unsigned turn = 0; turn < state->turns; ++turn) {
    state->runs++;
    record(state->name);
    if (turn + 1 < state->turns)
      coroutine_yield();
  }
}

static void finish_counter(void *arg) {
  Counter *state = arg;
  state->id = coroutine_id();
  state->runs++;
  record(state->name);
  coroutine_finish();

  // coroutine_finish() switches away from a non-main coroutine and must not
  // return to this completed job.
  state->runs++;
  record('!');
}

static void check_events(const char *expected, size_t expected_count) {
  CHECK(event_count == expected_count);
  for (size_t index = 0; index < expected_count; ++index)
    CHECK(events[index] == expected[index]);
}

int main(void) {
  Counter first = {.name = 'A', .turns = 2};
  Counter second = {.name = 'B', .turns = 2};
  Counter final = {.name = 'C', .turns = 1};

  coroutine_init();
  CHECK(coroutine_id() == 0);
  CHECK(coroutines_alive() == 1);

  coroutine_go(counter, &first);
  coroutine_go(counter, &second);
  CHECK(coroutines_alive() == 3);
  CHECK(event_count == 0);

  while (coroutines_alive() > 1)
    coroutine_yield();

  check_events("ABAB", 4);
  CHECK(first.id != 0);
  CHECK(second.id != 0);
  CHECK(first.id != second.id);
  CHECK(first.runs == first.turns);
  CHECK(second.runs == second.turns);
  CHECK(coroutines_alive() == 1);

  coroutine_go(finish_counter, &final);
  while (coroutines_alive() > 1)
    coroutine_yield();

  check_events("ABABC", 5);
  CHECK(final.id != 0);
  CHECK(final.id != first.id && final.id != second.id);
  CHECK(final.runs == final.turns);
  CHECK(first.runs == first.turns);
  CHECK(second.runs == second.turns);
  CHECK(coroutines_alive() == 1);

  coroutine_finish();
  return EXIT_SUCCESS;
}
