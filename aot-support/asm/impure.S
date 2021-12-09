        .arch armv7e-m
        .syntax unified
        .thumb
        .thumb_func
	.bss
	.align 8

impure_data:
	.space	256
	.size	impure_data, . - impure_data

	.data
	.global _impure_ptr
_impure_ptr:
	.word	impure_data
	.size	_impure_ptr, . - _impure_ptr
