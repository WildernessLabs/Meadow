# GDB debugging

## Getting started

Launch GDB using the command `arm-none-eabi-gdb-py -q`.

If you do it from the `WildernessLabs/Meadow/nuttx` directory, then
GDB will automatically load the `.gdbinit` and you will not need the
manual configuration steps below.

## Manual configuration

These steps are only needed if not launching from the NuttX working
directory as previously mentioned.

It can be helpful to know what's going on under the hood, so here's the
basic steps to get it working for Meadow.

* Load the necessary ELF symbols for kernel and user binaries

This is done using the `add-symbol-file` command:

```
add-symbol-file -readnow nuttx
add-symbol-file -readnow configs/stm32f777zit6-meadow/kernel/nuttx_user.elf
```

Added by default when using the `.gdbinit`init script.

* Load the Python scripts for extra commands

```
source gdb/Nuttx.py
source gdb/Nuttx_Tasks.py
```

Loaded by default when using the `.gdbinit` init script.

### Other helpful configuration scripts

* `set output-radix 16`

You can set this so GDB outputs integer data types as hexadecimal.
which is pretty helpful since that's the default representation
usually used for memory addresses and other constants in the
the memory map and linker scripts.

Set by default when using the `.gdbinit` init script.

* `set disassemble-next-line on`

If debugging using the `ni` or `si` assembly-level next/step
instructions, then this could be useful by telling GDB to print
the next assembly instruction on each successive step.

This is not set by default, use if working a lot with assembly.

## Commands

### Stack traces

Stack traces can be obtained using the `bt` command.

```
(gdb) bt
#0  pthread_cond_timedwait (cond=0x20032e10 <sleep_cond>, mutex=0x20032e04 <sleep_mutex>, abstime=0xc0191380) at pthread/pthread_condtimedwait.c:290
#1  0x08008a82 in STUB_pthread_cond_timedwait (nbr=0x6c, parm1=0x20032e10, parm2=0x20032e04, parm3=0xc0191380) at stubs/STUB_pthread_cond_timedwait.c:11
#2  0x08002142 in dispatch_syscall () at armv7-m/up_svcall.c:97
Backtrace stopped: previous frame identical to this frame (corrupt stack?)
```

If the stack traces crosses the kernel/user layer or shows sign of
being incomplete, then it can be useful to use the `nx_bt` command.

```
(gdb) nx_bt
#0 0x08017f1c pthread_cond_timedwait () at pthread/pthread_condtimedwait.c:290 
#1 0x08008a76 STUB_pthread_cond_timedwait () at stubs/STUB_pthread_cond_timedwait.c:11 
#2 0x0800212c dispatch_syscall () at armv7-m/up_svcall.c:97 
#3 0x080fb0ac sys_call3 () at /Users/joao/dev/WildernessLabs/Meadow/nuttx/include/arch/armv7-m/syscall.h:156 [syscall]
#4 0x080fb0c6 pthread_cond_timedwait () at proxies/PROXY_pthread_cond_timedwait.c:12 
```

It usually can show a little bit more information regarding the calls from
user space.

### Mapping from addresses to symbols

The `info line *<address>` command can be used to map from a
memory address to a source file/line.

```
(gdb) info line *0x800bf69
Line 83 of "common/up_task_start.c" starts at address 0x800bf68 <up_task_start+24> and ends at 0x800bf70 <sys_call4>.
```

### Registers

You can show the output of all registers under the current frame
using the `info reg` command (or `i r` for short).

```
(gdb) info reg
r0             0x20034a00          0x20034a00
r1             0x0                 0x0
r2             0x0                 0x0
r3             0x0                 0x0
r4             0xc0191890          0xc0191890
r5             0xc0191888          0xc0191888
r6             0xc018aa70          0xc018aa70
r7             0xc01918a8          0xc01918a8
r8             0xc0188f50          0xc0188f50
r9             0x0                 0x0
r10            0xc018a880          0xc018a880
r11            0xc0191b70          0xc0191b70
r12            0x8008b65           0x8008b65
sp             0xc0191878          0xc0191878
lr             0x8070fd9           0x8070fd9
pc             0x806a85e           0x806a85e <interp_exec_method_full+2542>
xpsr           0x61000000          0x61000000
msp            0xc01913a8          0xc01913a8
psp            0xc0191878          0xc0191878
control        0x3                 0x3
faultmask      0x0                 0x0
basepri        0x0                 0x0
primask        0x0                 0x0
fpscr          0x80000010          0x80000010
```

```
(gdb) i r sp pc lr
sp             0xc0191878          0xc0191878
pc             0x806a85e           0x806a85e <interp_exec_method_full+2542>
lr             0x8070fd9           0x8070fd9
```

### Breakpoints

There are multiple ways to set breakpoints, from using file 
paths and line numbers to explicit code addresses, which can
be done using the `b` command as shown below.

```
(gdb) b sched/pthread/pthread_condtimedwait.c:290
Breakpoint 1 at 0x8017f1c: file pthread/pthread_condtimedwait.c, line 290.
```

```
(gdb) b *0x8017f1c
Breakpoint 2 at 0x8017f1c: file pthread/pthread_condtimedwait.c, line 290.
```

### Disassembly

It can helpful to disassembly some piece of code, either using
a symbol or a memory address.

GDB provides a couple of commands to do this.

The first is the `disass <symbol>` command:

```
(gdb) disass up_pthread_start
Dump of assembler code for function up_pthread_start:
   0x0801da44 <+0>:	push	{r7, lr}
   0x0801da46 <+2>:	sub	sp, #8
   0x0801da48 <+4>:	add	r7, sp, #0
   0x0801da4a <+6>:	str	r0, [r7, #4]
   0x0801da4c <+8>:	str	r1, [r7, #0]
   0x0801da4e <+10>:	ldr	r3, [r7, #4]
   0x0801da50 <+12>:	ldr	r2, [r7, #0]
   0x0801da52 <+14>:	mov	r1, r3
   0x0801da54 <+16>:	movs	r0, #5
   0x0801da56 <+18>:	bl	0x801da1c <sys_call2>
   0x0801da5a <+22>:	nop
   0x0801da5c <+24>:	adds	r7, #8
   0x0801da5e <+26>:	mov	sp, r7
   0x0801da60 <+28>:	pop	{r7, pc}
End of assembler dump.
```

If you prefer to disassembly code from an address then use `x/<size>i`:

```
(gdb) x/10i 0x0801da44
   0x801da44 <up_pthread_start>:	push	{r7, lr}
   0x801da46 <up_pthread_start+2>:	sub	sp, #8
   0x801da48 <up_pthread_start+4>:	add	r7, sp, #0
   0x801da4a <up_pthread_start+6>:	str	r0, [r7, #4]
   0x801da4c <up_pthread_start+8>:	str	r1, [r7, #0]
   0x801da4e <up_pthread_start+10>:	ldr	r3, [r7, #4]
   0x801da50 <up_pthread_start+12>:	ldr	r2, [r7, #0]
   0x801da52 <up_pthread_start+14>:	mov	r1, r3
   0x801da54 <up_pthread_start+16>:	movs	r0, #5
   0x801da56 <up_pthread_start+18>:	bl	0x801da1c <sys_call2>
```

```
(gdb) x/10i $pc
=> 0x806a85e <interp_exec_method_full+2542>:	ldr.w	r1, [r7, #332]	; 0x14c
   0x806a862 <interp_exec_method_full+2546>:	cbz	r1, 0x806a8cc <interp_exec_method_full+2652>
   0x806a864 <interp_exec_method_full+2548>:	str	r3, [sp, #0]
   0x806a866 <interp_exec_method_full+2550>:	mov	r2, r11
   0x806a868 <interp_exec_method_full+2552>:	subs	r3, r6, #4
   0x806a86a <interp_exec_method_full+2554>:	add.w	r0, r8, #8
   0x806a86e <interp_exec_method_full+2558>:	bl	0x8069780 <interp_throw>
   0x806a872 <interp_exec_method_full+2562>:	ldr.w	r3, [r8, #12]
   0x806a876 <interp_exec_method_full+2566>:	cmp	r3, r11
   0x806a878 <interp_exec_method_full+2568>:	bne.w	0x806aaac <interp_exec_method_full+3132>
```

## Troubleshooting

* `info files`

This will show how the sections of the ELF executables are mapped
into memory. You can use this to troubleshoot weird issues like
breakpoints suddently not hitting.

```
(gdb) info files
Symbols from "WildernessLabs/Meadow/nuttx/nuttx".
Remote serial target in gdb-specific protocol:
Debugging a target over a serial line.
	While running this, GDB does not access memory from...
Local exec file:
	`WildernessLabs/Meadow/nuttx/nuttx', file type elf32-littlearm.
	Entry point: 0x8000310
	0x08000000 - 0x08020938 is .text
	0x08020938 - 0x08020940 is .ARM.exidx
	0x20020000 - 0x200201a6 is .data
	0x200201a8 - 0x20022864 is .bss
	0x08040000 - 0x08040030 is .userspace in WildernessLabs/Meadow/nuttx/configs/stm32f777zit6-meadow/kernel/nuttx_user.elf
	0x08040030 - 0x081a32a8 is .text in WildernessLabs/Meadow/nuttx/configs/stm32f777zit6-meadow/kernel/nuttx_user.elf
	0x081a32a8 - 0x081a32b0 is .ARM.exidx in WildernessLabs/Meadow/nuttx/configs/stm32f777zit6-meadow/kernel/nuttx_user.elf
	0x20030000 - 0x200316a0 is .data in WildernessLabs/Meadow/nuttx/configs/stm32f777zit6-meadow/kernel/nuttx_user.elf
	0x200316c0 - 0x2003cf30 is .bss in WildernessLabs/Meadow/nuttx/configs/stm32f777zit6-meadow/kernel/nuttx_user.elf
```

* `mov r0, r0`

If you start seeing weird output like repeated `movs	r0, r0` instructions  while disassemble code,
then reset the board and restart the debugging session.

```
(gdb) disass __start
Dump of assembler code for function __start:
=> 0x08000310 <+0>:	movs	r0, r0
   0x08000312 <+2>:	movs	r0, r0
   0x08000314 <+4>:	movs	r0, r0
   0x08000316 <+6>:	movs	r0, r0
```
 No newline at end of file
