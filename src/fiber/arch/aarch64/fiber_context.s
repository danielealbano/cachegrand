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
    stp d8,  d9,  [x0, #0x00]
    stp d10, d11, [x0, #0x10]
    stp d12, d13, [x0, #0x20]
    stp d14, d15, [x0, #0x30]

    stp x19, x20, [x0, #0x40]
    stp x21, x22, [x0, #0x50]
    stp x23, x24, [x0, #0x60]
    stp x25, x26, [x0, #0x70]
    stp x27, x28, [x0, #0x80]
    stp x29, x30, [x0, #0x90]

    str x30, [x0, #0xa0]

    mov x4, sp
    str x4, [x0, #0xb0]

    ret

.type fiber_context_set, @function
.global fiber_context_set
fiber_context_set:
    ldp d8,  d9,  [x0, #0x00]
    ldp d10, d11, [x0, #0x10]
    ldp d12, d13, [x0, #0x20]
    ldp d14, d15, [x0, #0x30]
    ldp x19, x20, [x0, #0x40]
    ldp x21, x22, [x0, #0x50]
    ldp x23, x24, [x0, #0x60]
    ldp x25, x26, [x0, #0x70]
    ldp x27, x28, [x0, #0x80]
    ldp x29, x30, [x0, #0x90]

    ldr x4, [x0, #0xa0]

    ldr x3, [x0, #0xb0]
    mov sp, x3

    ret x4

.type fiber_context_swap, @function
.global fiber_context_swap
fiber_context_swap:
    // str - Store Register (immediate) stores a word or a doubleword from a register to memory.
    // stp - Store Pair of Registers calculates an address from a base register value and an immediate offset, and stores two 32-bit words or two 64-bit doublewords to the calculated address, from two registers.
    // mov - Move between register and stack pointer
    // ldr - Load Register (immediate) loads a word or doubleword from memory and writes it to a register. The address that is used for the load is calculated from a base register and an immediate offset. For information about memory accesses, see Load/Store addressing modes.
    // ldp - Load Pair of Registers calculates an address from a base register value and an immediate offset, loads two 32-bit words or two 64-bit doublewords from memory, and writes them to two registers.
    // sub - Subtract (immediate) subtracts an optionally-shifted immediate value from a register value, and writes the result to the destination register.
    // br - Branch to Register branches unconditionally to an address in a register, with a hint that this is not a subroutine return.

    stp d8,  d9,  [x0, #0x00]
    stp d10, d11, [x0, #0x10]
    stp d12, d13, [x0, #0x20]
    stp d14, d15, [x0, #0x30]

    stp x19, x20, [x0, #0x40]
    stp x21, x22, [x0, #0x50]
    stp x23, x24, [x0, #0x60]
    stp x25, x26, [x0, #0x70]
    stp x27, x28, [x0, #0x80]
    stp x29, x30, [x0, #0x90]

    str x30, [x0, #0xa0]

    mov x4, sp
    str x4, [x0, #0xb0]

    ldp d8,  d9,  [x1, #0x00]
    ldp d10, d11, [x1, #0x10]
    ldp d12, d13, [x1, #0x20]
    ldp d14, d15, [x1, #0x30]
    ldp x19, x20, [x1, #0x40]
    ldp x21, x22, [x1, #0x50]
    ldp x23, x24, [x1, #0x60]
    ldp x25, x26, [x1, #0x70]
    ldp x27, x28, [x1, #0x80]
    ldp x29, x30, [x1, #0x90]

    ldr x4, [x1, #0xa0]

    ldr x3, [x1, #0xb0]
    mov sp, x3

    ret x4
