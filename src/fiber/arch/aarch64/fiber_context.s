/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
*/

.type fiber_context_get, @function
.global fiber_context_get
fiber_context_get:
    // Store Pair of Registers
    // Calculates an address from a base register value and an immediate offset
    // and store the calculated address, from two registers.
    stp     d15, d14, [sp, #-20*8]!    // Pre-Index
    stp     d13, d12, [sp, #2*8]
    stp     d11, d10, [sp, #4*8]
    stp     d9, d8,   [sp, #6*8]

    stp     x30, x29, [sp, #8*8]    // lr, fp
    stp     x28, x27, [sp, #10*8]
    stp     x26, x25, [sp, #12*8]
    stp     x24, x23, [sp, #14*8]
    stp     x22, x21, [sp, #16*8]
    stp     x20, x19, [sp, #18*8]

    // Adds a register value and an optionally-shifted register value,
    // and writes the result to the destination register.
    add     x19, sp, #9*8
    ret

.type fiber_context_set, @function
.global fiber_context_set
fiber_context_set:
    // Store Register (immediate)
    // Stores a word or a doubleword from a register to memory.
    str     x19, [x0]   // *old pointer stack

    // Subtract (shifted register)
    // Subtracts an optionally-shifted immediate value from a register value,
    // and writes the result to the destination register.
    sub     sp, x1, #9*8    // switch to newp sp

    // Load Pair of Registers
    // Calculates an address from a base register value and an immediate offset,
    // loads from memory, and writes them to two registers.
    ldp     x20, x19, [sp, #18*8]
    ldp     x22, x21, [sp, #16*8]
    ldp     x24, x23, [sp, #14*8]
    ldp     x26, x25, [sp, #12*8]
    ldp     x28, x27, [sp, #10*8]
    ldp     x30, x29, [sp, #8*8]    // lr, fp
    ldp     d9, d8,   [sp, #6*8]
    ldp     d11, d10, [sp, #4*8]
    ldp     d13, d12, [sp, #2*8]
    ldp     d15, d14, [sp], #20*8
    ret

.type fiber_context_swap, @function
.global fiber_context_swap
fiber_context_swap:
    stp     d15, d14, [sp, #-20*8]!
    stp     d13, d12, [sp, #2*8]
    stp     d11, d10, [sp, #4*8]
    stp     d9, d8,   [sp, #6*8]
    stp     x30, x29, [sp, #8*8]    // lr, fp
    stp     x28, x27, [sp, #10*8]
    stp     x26, x25, [sp, #12*8]
    stp     x24, x23, [sp, #14*8]
    stp     x22, x21, [sp, #16*8]
    stp     x20, x19, [sp, #18*8]

    add     x19, sp, #9*8
    str     x19, [x0]
    sub     sp, x1, #9*8

    ldp     x20, x19, [sp, #18*8]
    ldp     x22, x21, [sp, #16*8]
    ldp     x24, x23, [sp, #14*8]
    ldp     x26, x25, [sp, #12*8]
    ldp     x28, x27, [sp, #10*8]
    ldp     x30, x29, [sp, #8*8]    // lr, fp
    ldp     d9, d8,   [sp, #6*8]
    ldp     d11, d10, [sp, #4*8]
    ldp     d13, d12, [sp, #2*8]
    ldp     d15, d14, [sp], #20*8
    ret
