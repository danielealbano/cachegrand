/**
 * Copyright (C) 2018-2022 Vito Castellano
 * Copyright (C) 2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

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

    # Make room on the stack for the registers
    sub sp, sp, #0xb0

    # Save the registers from d8 to d15
    stp d8,  d9,  [sp, #0x00]
    stp d10, d11, [sp, #0x10]
    stp d12, d13, [sp, #0x20]
    stp d14, d15, [sp, #0x30]

    # Save the registers from x19 to x30
    stp x19, x20, [sp, #0x40]
    stp x21, x22, [sp, #0x50]
    stp x23, x24, [sp, #0x60]
    stp x25, x26, [sp, #0x70]
    stp x27, x28, [sp, #0x80]
    stp x29, x30, [sp, #0x90]

    # Save the LR register as program counter
    str x30, [sp, #0xa0]

    # Store the stack pointer in the first argument (x0)
    mov x4, sp
    str x4, [x0]

    # Load the stack pointer from the second argument (x1)
    ldr x3, [x1]
	mov sp, x3

    # Load the registers from d8 to d15
    ldp d8,  d9,  [sp, #0x00]
    ldp d10, d11, [sp, #0x10]
    ldp d12, d13, [sp, #0x20]
    ldp d14, d15, [sp, #0x30]

    # Load the registers from x19 to x30
    ldp x19, x20, [sp, #0x40]
    ldp x21, x22, [sp, #0x50]
    ldp x23, x24, [sp, #0x60]
    ldp x25, x26, [sp, #0x70]
    ldp x27, x28, [sp, #0x80]
    ldp x29, x30, [sp, #0x90]

    # Load the LR register as program counter
    ldr x4, [sp, #0xa0]

    # Restore the stack pointer
    add sp, sp, #0xb0

    ret x4

/**
 * The following section is added as the functions here don't require an executable stack, it's used to fix the
 * following warning:
 * /usr/bin/ld: warning: fiber_context.s.o: missing .note.GNU-stack section implies executable stack
 * /usr/bin/ld: NOTE: This behaviour is deprecated and will be removed in a future version of the linker
 **/
.section .note.GNU-stack,"",%progbits
