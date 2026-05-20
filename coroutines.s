.equ COROUTINES_CAPACITY, 10
.equ COROUTINE_STACK_SIZE, 1024 * 1024 // 1MB per coroutine
.equ TOTAL_STACK_MEMORY, COROUTINES_CAPACITY * COROUTINE_STACK_SIZE

.text
.global _start
.align 2

// X18 must not be used
// X29 (frame pointer) must point to a valid stack frame
// X30 (link register) contains return address
// SP must be 16-byte aligned

// void coroutine_go(void *(void))
// void coroutine_yield(void)
// void coroutine_init(void)
// TODO: void coroutine_finish(void)

// X1 is the pointer to data, X2 is length
print:
  stp X29, X30, [SP, #-16]!
  mov X29, SP
  mov X0, #1  // stdout
  mov X16, #4 // write
  svc #0x80
  ldp X29, X30, [SP], #16
  ret


endl:
  stp X29, X30, [SP, #-16]!
  mov X29, SP
  adrp X1, newline@PAGE
  add X1, X1, newline@PAGEOFF
  mov X2, #newline_len
  bl print
  ldp X29, X30, [SP], #16
  ret


coroutine_ret:
  stp X29, X30, [SP, #-16]!
  mov X29, SP
  adrp X1, coroutine_returned_msg@PAGE
  add X1, X1, coroutine_returned_msg@PAGEOFF
  mov X2, #coroutine_returned_msg_len
  bl print
  mov X0, #69
  mov X16, #1
  svc #0x80    // exit(69)
  ldp X29, X30, [SP], #16
  ret


coroutine_init:
  stp X29, X30, [SP, #-16]!
  mov X29, SP

  adrp X1, coroutines_count@PAGE
  add X1, X1, coroutines_count@PAGEOFF
  ldr X2, [X1]
  cmp X2, COROUTINES_CAPACITY
  b.ge .init_capacity_exceeded

  add X5, X2, #1 // coroutines_count + 1
  str X5, [X1]   // update the coroutines_count global

  ldp X29, X30, [SP], #16
  ret

.init_capacity_exceeded:
  adrp X1, capacity_exceeded_msg@PAGE
  add X1, X1, capacity_exceeded_msg@PAGEOFF
  mov X2, #capacity_exceeded_msg_len
  bl print
  ldp X29, X30, [SP], #16
  ret


// X0 is the address of coroutine job
coroutine_go:
  stp X29, X30, [SP, #-16]!
  mov X29, SP

  adrp X1, coroutines_count@PAGE
  add X1, X1, coroutines_count@PAGEOFF
  ldr X2, [X1]
  cmp X2, COROUTINES_CAPACITY
  b.ge .go_capacity_exceeded

  add X5, X2, #1 // coroutines_count + 1
  str X5, [X1]   // update the coroutines_count global

  // Allocate stack for coroutine
  mov X3, COROUTINE_STACK_SIZE
  mul X4, X5, X3 // SP offset for coroutine = (coroutines_count + 1) * COROUTINE_STACK_SIZE

  adrp X1, stacks@PAGE
  add X1, X1, stacks@PAGEOFF
  add X1, X1, X4 // SP

  // Store SP
  // X2 = coroutines_count
  // X5 = coroutines_count + 1
  adrp X3, contexts_sp@PAGE
  add X3, X3, contexts_sp@PAGEOFF
  mov X6, #8
  mul X4, X2, X6
  add X7, X4, X3
  str X1, [X7] // contexts_sp[coroutines_count] = SP
  
  adrp X3, contexts_fp@PAGE
  add X3, X3, contexts_fp@PAGEOFF
  add X7, X4, X3
  str X1, [X7] // contexts_fp[coroutines_count] = SP

  adrp X3, contexts_lr@PAGE
  add X3, X3, contexts_lr@PAGEOFF
  add X7, X4, X3
  adrp X1, coroutine_ret@PAGE
  add X1, X1, coroutine_ret@PAGEOFF
  str X1, [X7] // contexts_lr[coroutines_count] = coroutine_ret
  
  adrp X3, contexts_pc@PAGE
  add X3, X3, contexts_pc@PAGEOFF
  add X7, X4, X3
  str X0, [X7] // contexts_pc[coroutines_count] = <address of coroutine function>

  str X0, [SP, #-16]!
  adrp X1, coroutine_added_msg@PAGE
  add X1, X1, coroutine_added_msg@PAGEOFF
  mov X2, #coroutine_added_msg_len
  bl print
  ldr X0, [SP]
  bl print_int
  bl endl

  add SP, SP, #16
  ldp X29, X30, [SP], #16
  ret

.go_capacity_exceeded:
  adrp X1, capacity_exceeded_msg@PAGE
  add X1, X1, capacity_exceeded_msg@PAGEOFF
  mov X2, #capacity_exceeded_msg_len
  bl print
  ldp X29, X30, [SP], #16
  ret


coroutine_yield:
  // brk #0
  stp X29, X30, [SP, #-16]!
  mov X5, X29 // Remember the caller's FP
  mov X29, SP

  // Load the current coroutine index
  adrp X0, current_coroutine_index@PAGE
  add X0, X0, current_coroutine_index@PAGEOFF // X0 - address of current_coroutine_index
  ldr X1, [X0]                                // X1 = current coroutine index
  mov X6, #8
  mul X4, X1, X6

  // Save current coroutine's context.
  adrp X3, contexts_sp@PAGE
  add X3, X3, contexts_sp@PAGEOFF
  add X7, X4, X3
  add X6, SP, #16 // SP
  str X6, [X7]    // contexts_sp[current_coroutine_index] = SP

  adrp X3, contexts_fp@PAGE
  add X3, X3, contexts_fp@PAGEOFF
  add X7, X4, X3
  str X5, [X7] // contexts_fp[current_coroutine_index] = X5 (caller's FP)

  adrp X3, contexts_pc@PAGE
  add X3, X3, contexts_pc@PAGEOFF
  add X7, X4, X3
  str X30, [X7] // contexts_pc[current_coroutine_index] = return address (into the coroutine)

  // Increment the current coroutine index, wrapping around the capacity.
  add X1, X1, #1  // next_cor_index

  adrp X5, coroutines_count@PAGE
  add X5, X5, coroutines_count@PAGEOFF
  ldr X6, [X5]

  // brk #6

  udiv X3, X1, X6     // X3 = next_cor_index / coroutines_count
  msub X1, X6, X3, X1 // X4 = next_cor_index - coroutines_count * X3

  str X1, [X0]

  // Load the next coroutine context.
  mov X6, #8
  mul X4, X1, X6

  adrp X3, contexts_sp@PAGE
  add X3, X3, contexts_sp@PAGEOFF
  add X7, X4, X3
  ldr X6, [X7]    // X6 = contexts_sp[next_cor_index]

  adrp X3, contexts_fp@PAGE
  add X3, X3, contexts_fp@PAGEOFF
  add X7, X4, X3
  ldr X5, [X7] // X5 = contexts_fp[next_cor_index]

  adrp X3, contexts_pc@PAGE
  add X3, X3, contexts_pc@PAGEOFF
  add X7, X4, X3
  ldr X8, [X7] // X4 = contexts_pc[next_cor_index] is the return address (into the coroutine)

  adrp X3, contexts_lr@PAGE
  add X3, X3, contexts_lr@PAGEOFF
  add X7, X4, X3
  ldr X3, [X7]

  // add SP, SP, #16
  mov SP, X6
  mov X29, X5
  mov X30, X3
  br X8
  // ldp X29, X30, [SP], #16
  // ret



// X0 is the number to count from
counter:
  stp X29, X30, [SP, #-16]!
  mov X29, SP
  mov X15, #10
  mov X1, #0
  cmp X15, X1
  b.lt .counter_exit

  str X15, [SP, #-16]!

.counter_loop:
  adrp X1, current_coroutine_index@PAGE
  add X1, X1, current_coroutine_index@PAGEOFF
  ldr X0, [X1]
  bl print_int


  ldr X15, [SP]
  mov X0, X15
  bl print_int

  mov X0, #1   // STDOUT
  adrp X1, newline@PAGE
  add X1, X1, newline@PAGEOFF
  mov X2, #newline_len
  mov X16, #4
  svc #0x80

  bl coroutine_yield

  ldr X15, [SP]
  sub X15, X15, #1
  str X15, [SP]
  cbnz X15, .counter_loop

.counter_exit:
  add SP, SP, #16
  ldp X29, X30, [SP], #16
  // FIXME: coroutines must exit via coroutine_finish!
  ret


// X0 is the doubleword number to be printed.
print_int:
  stp X29, X30, [SP, #-16]!
  mov X29, SP

  sub	SP, SP, #32  // allocate 21 bytes for the buffer, 32 for alignment
  mov X1, #20      // current buffer position
  mov X2, #10      // divisor

.extract_digit:
  udiv X3, X0, X2     // X3 = X0 / X2
  msub X4, X2, X3, X0 // X4 = X0 - X2 * X3
  add X4, X4, #48     // X4 = X4 + '0'
  strb W4, [SP, X1]   // buf[buf_pos] = '0' + digit
  sub X1, X1, 1
  mov X0, X3
  cbnz X3, .extract_digit // while (X0 != 0)

  mov X0, #1
  mov X3, #20
  sub X2, X3, X1
  add X1, SP, X1
  add X1, X1, 1
  mov X16, #4
  svc #0x80

  add SP, SP, #32
  ldp X29, X30, [SP], #16
  ret

_start:
  bl coroutine_init

  adrp X0, counter@PAGE
  add X0, X0, counter@PAGEOFF
  bl coroutine_go

  adrp X0, counter@PAGE
  add X0, X0, counter@PAGEOFF
  bl coroutine_go

.forever:
  bl coroutine_yield
  b .forever

  mov X0, #0
  mov X16, #1
  svc #0x80    // exit(0)

.data
helloworld: .ascii "Hello World!\n"
.set helloworld_len, . - helloworld
newline: .ascii "\n"
.set newline_len, . - newline
capacity_exceeded_msg: .ascii "Coroutines capacity exceeded!\n"
.set capacity_exceeded_msg_len, . - capacity_exceeded_msg
coroutine_added_msg: .ascii "Coroutine added: "
.set coroutine_added_msg_len, . - coroutine_added_msg
coroutine_returned_msg: .ascii "Coroutine returned, but it mustn't do it!\n"
.set coroutine_returned_msg_len, . - coroutine_returned_msg

.bss
.align 4
coroutines_count:
  .skip 8
current_coroutine_index:
  .skip 8
.align 4
stacks:
  .skip TOTAL_STACK_MEMORY
contexts_sp:
  .skip COROUTINES_CAPACITY * 8
contexts_fp:
  .skip COROUTINES_CAPACITY * 8
contexts_lr:
  .skip COROUTINES_CAPACITY * 8
contexts_pc:
  .skip COROUTINES_CAPACITY * 8
