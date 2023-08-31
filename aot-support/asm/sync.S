	.arch armv7e-m
	.syntax unified
	.thumb
	.thumb_func
	.fpu vfp

	.type   __sync_synchronize, %function
	.globl  __sync_synchronize
__sync_synchronize:
	dmb	ish
	bx	lr
	.size	__sync_synchronize, .-__sync_synchronize

	.type   __sync_lock_test_and_set_4, %function
	.globl  __sync_lock_test_and_set_4
	.align  4
__sync_lock_test_and_set_4:
	movs	r1, #1
1:
	ldrexb	r2, [r0]	/* Grab lock value */
	cmp     r2, r1          /* Check if locked */
	itt	eq
	moveq	r0, #1		/* Tell caller we're locked */
	bxeq	lr		/* Return */

	strexb	r2, r1, [r0]	/* Set and check */
	cmp	r2, r1		/* Set ? */
	beq	1b		/* Look again */

	dmb     		/* Barrier time */
	movs	r0, #0		/* Tell caller we're not locked */
	bx	lr		/* Return */

	.size	__sync_lock_test_and_set_4, .-__sync_lock_test_and_set_4

	.type   __sync_val_compare_and_swap_4, %function
	.globl  __sync_val_compare_and_swap_4
	.align  4
__sync_val_compare_and_swap_4:
	push	{r3, r4, lr}
	dmb	ish
	ldrex	r3, [r0]
	teq	r3, r1
	it	eq
	strexeq r4, r2, [r0]
	mov.n	r0, r3
	pop	{r3, r4, pc}
	.size	__sync_val_compare_and_swap_4, .-__sync_val_compare_and_swap_4
