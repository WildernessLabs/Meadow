.thumb

.globl _interp_to_native_trampoline
_interp_to_native_trampoline:
    push    {r4, r5, r7, lr}
    mov     r4, r8
    push    {r4}
    mov     r7, sp
    sub     sp, sp, #12
    sub     r4, r7, #4
    str     r1, [r4]
    sub     r4, r7, #4
    sub     r4, r4, #4
    str     r0, [r4]
    ldr     r3, [r1, #80]
    mov     r4, sp
    sub     r4, r4, r3
    mov     sp, r4
    mov     r0, sp
    ldr     r1, [r1, #84]
1:
    cmp     r3, #0
    beq     2f
    ldr     r2, [r1]
    str     r2, [r0]
    add     r0, r0, #4
    add     r1, r1, #4
    sub     r3, r3, #4
    b       1b
2:
    sub     r4, r7, #4
    ldr     r5, [r4]
    ldr     r0, [r5]
    ldr     r1, [r5, #4]
    ldr     r2, [r5, #8]
    ldr     r3, [r5, #12]
    vldr    d0, [r5, #16]
    vldr    d1, [r5, #24]
    vldr    d2, [r5, #32]
    vldr    d3, [r5, #40]
    vldr    d4, [r5, #48]
    vldr    d5, [r5, #56]
    vldr    d6, [r5, #64]
    vldr    d7, [r5, #72]
    sub     r4, r7, #4
    sub     r4, r4, #4
    ldr     r5, [r4]
    blx     r5
    sub     r4, r7, #4
    ldr     r5, [r4]
    str     r0, [r5]
    str     r1, [r5, #4]
    str     r2, [r5, #8]
    str     r3, [r5, #12]
    vstr    d0, [r5, #16]
    vstr    d1, [r5, #24]
    vstr    d2, [r5, #32]
    vstr    d3, [r5, #40]
    vstr    d5, [r5, #56]
    vstr    d6, [r5, #64]
    vstr    d7, [r5, #72]
    mov     sp, r7
    pop     {r4}
    mov     r8, r4
    pop     {r4, r5, r7, pc}

/* ORIG ARM:
    push    {r7, r8, lr}
    mov     r7, sp
    sub     sp, sp, #12
    str     r1, [r7, #-4]
    str     r0, [r7, #-8]
    ldr     r3, [r1, #80]
    sub     sp, sp, r3
    mov     r0, sp
    ldr     r1, [r1, #84]

1:
    cmp     r3, #0
    beq     2f
    ldr     r2, [r1]
    str     r2, [r0]
    add     r0, r0, #4
    add     r1, r1, #4
    sub     r3, r3, #4
    b       1b

2:
    ldr     r12, [r7, #-4]
    ldr     r0, [r12]
    ldr     r1, [r12, #4]
    ldr     r2, [r12, #8]
    ldr     r3, [r12, #12]
    vldr    d0, [r12, #16]
    vldr    d1, [r12, #24]
    vldr    d2, [r12, #32]
    vldr    d3, [r12, #40]
    vldr    d4, [r12, #48]
    vldr    d5, [r12, #56]
    vldr    d6, [r12, #64]
    vldr    d7, [r12, #72]
    ldr     r12, [r7, #-8]
    blx     r12
    ldr     r12, [r7, #-4]
    str     r0, [r12]
    str     r1, [r12, #4]
    str     r2, [r12, #8]
    str     r3, [r12, #12]
    vstr    d0, [r12, #16]
    vstr    d1, [r12, #24]
    vstr    d2, [r12, #32]
    vstr    d3, [r12, #40]
    vstr    d4, [r12, #48]
    vstr    d5, [r12, #56]
    vstr    d6, [r12, #64]
    vstr    d7, [r12, #72]
    mov     sp, r7
    pop     {r7, r8, pc}
    */

.globl _native_to_interp_trampoline
_native_to_interp_trampoline:
    push    {r4, r7, lr}
    mov     r7, sp
    sub     sp, sp, #88
    str     r0, [sp]
    str     r1, [sp, #4]
    str     r2, [sp, #8]
    str     r3, [sp, #12]
    vstr    d0, [sp, #16]
    vstr    d1, [sp, #24]
    vstr    d2, [sp, #32]
    vstr    d3, [sp, #40]
    vstr    d4, [sp, #48]
    vstr    d5, [sp, #56]
    vstr    d6, [sp, #64]
    vstr    d7, [sp, #72]
    add     r0, r7, #4
    add     r0, r0, #4
    str     r0, [sp, #84]
    mov     r0, sp
    mov     r4, r12
    ldr     r1, [r4, #8]
    ldr     r4, [r4]
    blx     r4
    ldr     r0, [sp]
    ldr     r1, [sp, #4]
    ldr     r2, [sp, #8]
    ldr     r3, [sp, #12]
    vldr    d0, [sp, #16]
    vldr    d1, [sp, #24]
    vldr    d2, [sp, #32]
    vldr    d3, [sp, #40]
    vldr    d4, [sp, #48]
    vldr    d5, [sp, #56]
    vldr    d6, [sp, #64]
    vldr    d7, [sp, #72]
    mov     sp, r7
    pop     {r4, r7, pc}
 /* ORIG ARM:   
    push    {r7, lr}
    mov     r7, sp
    sub     sp, sp, #88     ; 0x58
    str     r0, [sp]
    str     r1, [sp, #4]
    str     r2, [sp, #8]
    str     r3, [sp, #12]
    vstr    d0, [sp, #16]
    vstr    d1, [sp, #24]
    vstr    d2, [sp, #32]
    vstr    d3, [sp, #40]   ; 0x28
    vstr    d4, [sp, #48]   ; 0x30
    vstr    d5, [sp, #56]   ; 0x38
    vstr    d6, [sp, #64]   ; 0x40
    vstr    d7, [sp, #72]   ; 0x48
    add     r0, r7, #8
    str     r0, [sp, #84]   ; 0x54
    mov     r0, sp
    ldr     r1, [r12, #4]
    ldr     r12, [r12]
    blx     r12
    ldr     r0, [sp]
    ldr     r1, [sp, #4]
    ldr     r2, [sp, #8]
    ldr     r3, [sp, #12]
    vldr    d0, [sp, #16]
    vldr    d1, [sp, #24]
    vldr    d2, [sp, #32]
    vldr    d3, [sp, #40]   ; 0x28
    vldr    d4, [sp, #48]   ; 0x30
    vldr    d5, [sp, #56]   ; 0x38
    vldr    d6, [sp, #64]   ; 0x40
    vldr    d7, [sp, #72]   ; 0x48
    mov     sp, r7
    pop     {r7, pc}
*/

you_a_tramp:
push    {r5}
ldr     r5, [pc]
ldr     r5, [r5, #12]
mov     r12, r15
pop     {r5}
bx      r12