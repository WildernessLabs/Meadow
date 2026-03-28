# Track 09: Mono Mini Test Suite — Handoff

## Status: IN PROGRESS (2026-03-28)

### Phase 1 Results (mono_llvm_only=TRUE, interpreter only)

**7 suites, 379 tests, 3 failures — 98.9% pass rate**

Completed with basic/arrays/calls/float/long/math/exceptions suites.
Objects, generics, gshared skipped due to gsharedvt trampoline dependency.

### Phase 2: Thumb2 Trampoline Port (2026-03-28)

**Goal:** Disable `mono_llvm_only` by porting all ARM trampolines from A32 to Thumb2.

### Phase 3: Full Suite Results with Per-Test Exception Handling (2026-03-28)

Modified TestDriver.cs to catch per-test exceptions (upstream crashes entire suite on first failure).
Also fixed: `GetCustomAttributes()` crashes on NuttX — skip that path in exclude logic.

**10 suites, 735 tests, 22 failures — 97.0% pass rate**

| Suite | Tests | Failed | Notes |
|-------|-------|--------|-------|
| basic | 134 | 0 | Arithmetic, conversions, control flow |
| arrays | 36 | 2 | `multidym_array_with_negative_lower_bound` (NullRef), `intptr_array_cast` (wrong result) |
| basic-calls | 27 | 0 | Method calls |
| basic-float | 58 | 0 | Float/double math, MathF intrinsics |
| basic-long | 97 | 0 | 64-bit integer ops |
| basic-math | 27 | 1 | `atan_precision` — ARM atan2 precision edge case |
| objects | 105 | 1 | `mul_ovf_regress_36052` (NullRef — unsafe pointer ops) |
| exceptions | 86 | 3 | `invalid_unbox_arrays`, `rethrow_stacktrace`, `ldflda_null_pointer` (all NullRef) |
| generics | 78 | 2 | `delegate_callvirt_fullaot` (MustBeDelegate), `constrained_vtype_box` (wrong result) |
| gshared | 87 | 13 | See breakdown below |

**gshared failures (13):**
- `gsharedvt_in_delegates_reflection` — Arg_MustBeDelegate (Delegate.CreateDelegate)
- `open_delegate` — Arg_MustBeDelegate (Delegate.CreateDelegate)
- `begin_end_invoke` — PlatformNotSupported (expected — .NET doesn't support BeginInvoke)
- `arm64_small_stack_args`, `arm64_vtype_stack_args` — NullRef (ARM64 tests on ARM32, irrelevant)
- `signextension_{sbyte,byte,short,ushort,int,uint,long,ulong}` — 8× NullRef (gsharedvt generic instantiation via Activator.CreateInstance + MakeGenericType)

### Failure Categories

**Category A: Delegate.CreateDelegate (3 tests)**
`Arg_MustBeDelegate` — `Delegate.CreateDelegate()` fails when passed generic delegate types via reflection. `RuntimeType.IsDelegate()` may return false for generic delegate instantiations, or the CreateDelegate flow has another issue. Affects: generics (1), gshared (2).

**Category B: gsharedvt Generic Instantiation (8 tests)**
All 8 signextension tests use `Activator.CreateInstance(typeof(SEClass<>).MakeGenericType(...))` — open generic instantiation at runtime. The NullRef suggests `MakeGenericType` or `CreateInstance` fails silently. These exercise gsharedvt (generic sharing for value types).

**Category C: NullRef in Exception-Expecting Tests (4 tests)**
Tests that catch specific exceptions (IndexOutOfRange, InvalidCast, NullRef) get NullRef thrown at an unexpected point:
- `multidym_array_with_negative_lower_bound` — Array.CreateInstance with negative bounds
- `invalid_unbox_arrays` — foreach over jagged array as flat
- `ldflda_null_pointer` — null field address access
- `mul_ovf_regress_36052` — unsafe fixed/stackalloc pointer ops

**Category D: StackTrace Access (1 test)**
`rethrow_stacktrace` — `StackTrace.ToString()` crashes on NuttX (known issue: causes nested exception). Need to implement missing System.Native functions for stack walking.

**Category E: Expected/Irrelevant (3 tests)**
- `begin_end_invoke` — PlatformNotSupported (correct: .NET removed BeginInvoke)
- `arm64_small_stack_args`, `arm64_vtype_stack_args` — ARM64 tests on ARM32

**Category F: Value Failures (3 tests)**
- `atan_precision` — ARM atan2 precision edge case
- `intptr_array_cast` — IntPtr array cast (returns 1 instead of 0)
- `constrained_vtype_box` — constrained vtype boxing (returns 1 instead of 0)

### What Was Done (Phase 2)

**Core change:** Disabled `mono_llvm_only` on NuttX, enabled arch trampolines.

1. **tramp-arm.c** — Ported 7 trampoline functions to Thumb2:
   - `mono_arch_get_unbox_trampoline` — ARM_JUMP_REG_PARM, CODE_ADDR
   - `mono_arch_get_static_rgctx_trampoline` — ARM_JUMP_REG_PARM2, CODE_ADDR
   - `mono_arch_get_ftnptr_arg_trampoline` — same pattern
   - `mono_arch_create_generic_trampoline` — ARM_LOAD_RELPC, ARM_CALL_REG, ARMDISP_LDRPC backpatch, CODE_ADDR
   - `mono_arch_create_specific_trampoline` — ARM_JUMP_REG_PARMA, short_branch ifdef, CODE_ADDR
   - `mono_arch_create_rgctx_lazy_fetch_trampoline` — ARM_LOAD_RELPC, ARM_JUMP_REG_PARM, CODE_ADDR
   - `mono_arch_create_sdb_trampoline` — ARM_LOAD_RELPC, CODE_ADDR

2. **mini-arm.c** — Fixed 4 functions for Thumb2:
   - `arm_patch_general` — Added Thumb2 branch patching (B.W, BL, B<cond>.W, 16-bit B/Bcond)
   - `emit_thunk` — Compact 12-byte Thumb2 thunk (LDR.W + BX + NOP + literal)
   - `arm_emit_value_and_patch_ldr` — Thumb2 PC-relative offset encoding
   - `mono_arch_build_imt_trampoline` — CODE_ADDR return

3. **tramp-arm-gsharedvt.c** — Ported gsharedvt trampoline:
   - ARM_LOAD_RELPC for function pointer loading
   - ARM_CALL_REG for indirect calls
   - Fixed LR setup offset (10 instead of 4 for Thumb2)
   - CODE_ADDR return

4. **arm-codegen.h** — Added A32 versions of Thumb2 helper macros:
   - `ARM_LOAD_RELPC`, `ARM_CALL_REG`, `ARM_JUMP_REG_PARM*`, `CODE_ADDR`, `ARMDISP_LDRPC`

5. **driver.c** — Removed `mono_llvm_only = TRUE` for NuttX in INTERP_ONLY mode

6. **mini-trampolines.c** — Removed `HOST_NUTTX` from trampoline disable guard

7. **aot-runtime.c** — `mono_aot_get_unbox_trampoline` DISABLE_AOT stub redirects to `mono_arch_get_unbox_trampoline`

### What Was Done (Phase 3)

1. **TestDriver.cs** — Local copy with two modifications:
   - Added try/catch around `methods[i].Invoke()` so individual test failures don't crash the suite
   - Removed `GetCustomAttributes()` from exclude path (crashes on NuttX with NullRef)

### Thumb2 Trampoline Porting Patterns

| A32 Pattern | Thumb2 Equivalent | Notes |
|---|---|---|
| `ARM_LDR_IMM(code,R,PC,0); ARM_B(code,0); literal` | `ARM_LOAD_RELPC(code,R); literal` | PC-relative literal load |
| `ARM_MOV_REG_REG(code,LR,PC); emit_bx(code,R)` | `ARM_CALL_REG(code,R)` | Indirect call |
| `code - load_addr - 8` (backpatch offset) | `code - load_addr - 8 + ARMDISP_LDRPC` | ARMDISP_LDRPC=4 for Thumb2, 0 for A32 |
| `return buf` | `return CODE_ADDR(buf)` | CODE_ADDR sets thumb bit on Thumb2 |
| `ARM_LDR_IMM+ARM_B+literal+emit_bx` | `ARM_JUMP_REG_PARM(code,R,addr)` | Load literal + BX |

### Files Modified (in runtime repo)

- `src/mono/mono/mini/tramp-arm.c` — 7 trampoline functions ported to Thumb2
- `src/mono/mono/mini/mini-arm.c` — arm_patch_general, emit_thunk, arm_emit_value_and_patch_ldr, IMT trampoline
- `src/mono/mono/mini/tramp-arm-gsharedvt.c` — gsharedvt trampoline Thumb2 port
- `src/mono/mono/mini/mini-trampolines.c` — removed HOST_NUTTX trampoline disable
- `src/mono/mono/mini/driver.c` — removed mono_llvm_only=TRUE for NuttX
- `src/mono/mono/mini/aot-runtime.c` — unbox trampoline redirect
- `src/mono/mono/mini/mini-runtime.c` — reverted HOST_NUTTX GSHAREDVT guard
- `src/mono/mono/mini/llvmonly-runtime.c` — reverted to upstream (all HOST_NUTTX changes removed)
- `src/mono/mono/arch/arm/arm-codegen.h` — A32 helper macros

### Files Modified (in test runner)

- `conductor/tracks/mono-upgrade-09-tests/test-runner-app/TestDriver.cs` — Local copy with per-test try/catch
- `conductor/tracks/mono-upgrade-09-tests/test-runner-app/Program.cs` — Test runner with syslog output

### Renode Script Note

After any Mono runtime rebuild, check `.mono_bss` address:
```bash
arm-none-eabi-objdump -h build/dotnet10/nuttx_user.elf | grep mono_bss
```
Current values: start=0xC01AF450, size=0x50BB0 (330,672 bytes).
Update `meadow-dotnet10-headless.resc` and regenerate `mono_bss_zero.bin` if changed.
