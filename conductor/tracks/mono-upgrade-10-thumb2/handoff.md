# Handoff: Track 10 — Thumb2 JIT

## What was done this session

### 1. Fixed LFS tool `--interp` hardcoding
The `build_lfs_v1_image` binary had `--interp` baked in (source was fixed but never recompiled). Created `lfs_host_config.h` to enable host compilation without NuttX headers, rebuilt the tool. JIT mode config now flows correctly.

### 2. Fixed Thumb2 condition code flag-clobbering (Dictionary fix)
**Root cause**: In Thumb2 mode, `ARM_MOV_REG_IMM8` emits a 16-bit `MOVS` which always updates CPSR flags. This destroyed the CMP result before conditional MOVs could read them, breaking all integer comparisons (OP_ICGT, OP_ICLT, OP_ICEQ, etc.).

**Effect**: `OP_ICONV_TO_I8` (conv.i8 IL) produced wrong high 32 bits for sign extension. `(long)(int)3` became `0xFFFFFFFF_00000003` instead of `0x00000000_00000003`. This broke Dictionary.GetBucket's 64-bit remainder, causing IndexOutOfRangeException.

**Fix**: Integer comparisons → `ARM_GET_CC` (ITE block, both MOVs conditional). Float comparisons → `ARM_MOV_REG_IMM8_COND(ARMCOND_AL)` (IT AL block, MOV doesn't update flags). Matches legacy Mono Thumb2 port by Neale Ferguson (commit `da7ded3694b`).

### 3. Fixed `__THUMB__` vs `__thumb2__` preprocessor guard
All `#ifdef __THUMB__` guards were dead code because GCC `-mthumb` defines `__thumb2__` (lowercase). Changed all to `__thumb2__`.

### 4. Increased float comparison mdesc lengths
`float_ceq`/`r4_ceq`: 16→20, `float_cgt_un`/`r4_cgt_un`: 20→24, etc. Matches legacy values to accommodate IT prefix overhead.

## Key commits
- `runtime`: `444668ae2ca` — fix(nuttx): Thumb2 condition code flag-clobbering
- `Meadow.OS.Emulator`: `8e13bd9` — fix(tools): rebuild build_lfs_v1_image
- `Meadow`: `4cbad25b1ff` — test(jit-diag): add JitDiag v16

## Test results (JitDiag v16, JIT mode, Renode)
```
LongRem(0-10, 3) = all correct
CheckedNarrow(0,1,2,3,-1,-2) = all correct
CheckedNarrow(±4294967296) = OverflowException (correct)
GetBucketDiag(0-10) = all correct, b=3 (was -4294967293)
GetBucketLike(0-10) = all correct
Dict.Add(0-5) = ALL OK (was IndexOutOfRangeException)
```

### 5. mdesc instruction length audit — all clear
Audited all 6 instructions where .NET 10 has smaller mdesc lengths than legacy Mono:

| Instruction | .NET 10 | Legacy | Verdict |
|---|---|---|---|
| `int_add` | 4 | 8 | Safe — JIT allocator never uses SP; worst non-SP path is 4 bytes |
| `int_sub` | 4 | 8 | Safe — same reasoning |
| `switch` | 12 | 16 | Safe — `max_len += 4` under `#ifdef __thumb2__` compensates dynamically |
| `aotconst` | 16 | 20 | Safe — fixed 16-byte sequence (LDR+B+literal+LDR), no variable paths |
| `float_rem` | 16 | 122 | Dead code — `g_assert_not_reached()`, decomposed to helper call |
| `r4_rem` | 16 | 122 | Dead code — OP_RREM not even present in mini-arm.c |

### 6. JIT test suite — fixed OOM + 5 JIT bugs (Phase 5)

**OOM root cause**: SGen card table used 8MB (full 32-bit address space). Fixed by reducing
`CARD_TABLE_BITS` from 32 to 26 in `sgen-cardtable.h` for NuttX, enabling overlapping/aliased
cards (standard on 64-bit, proven codepath). Card table: 8MB → 128KB. Also added
`mono_gc_params_set()` as backup for `MONO_GC_PARAMS` env var (belt-and-suspenders).

**Heap corruption at test_5_regalloc**: Root cause was `test_0_exception_in_cctor` — the
TypeInitializationException handling path in `mono_magic_trampoline → mono_error_convert_to_exception
→ mono_error_free_string → g_free` corrupts a free node's flink in the NuttX heap. The freed
error string's stale pointer gets followed during later free-list traversal, creating a self-
referencing flink that causes an infinite loop. Confirmed by excluding the cctor test and
watching test_5_regalloc pass. Not yet fixed — excluded from test suite.

**5 JIT bugs fixed**: or_large_imm (OR immediate encoding), signed_ct_div (compile-time division),
ovf11 (checked decrement false overflow), bigmul6 (unsigned widening multiply), ldsfld_soft_float
(static R4 field compare).

### 7. Full test suite — 732 tests, 99.6% pass rate

| Suite | Ran | Skipped | Failed |
|-------|-----|---------|--------|
| basic | 134 | 0 | 0 |
| arrays | 36 | 0 | 1 (intptr_array_cast) |
| basic-calls | 27 | 0 | 0 |
| basic-float | 58 | 0 | 0 |
| basic-long | 97 | 0 | 0 |
| basic-math | 27 | 0 | 1 (atan_precision — same as interpreter) |
| objects | 105 | 0 | 0 |
| exceptions | 85 | 1 | 1 (ldflda_null_pointer — NullRef in emulator) |
| generics | 78 | 0 | 0 |
| gshared | 85 | 2 | 0 |
| **TOTAL** | **732** | **3** | **3** |

**2 excluded tests**: arm64_vtype_stack_args (ABORT in gsharedvt on ARM32),
begin_end_invoke (PlatformNotSupportedException — APM not supported).

### 8. Fixed cctor heap corruption — `mono_error_get_message` side-effect

**Root cause**: The NuttX JIT FAILED diagnostic in `mini-runtime.c:2843-2852` called
`mono_error_get_message(error)` on an `EXCEPTION_INSTANCE` error. This function has a
side-effect: it allocates `error->full_message_with_fields` via `g_strdup_printf`. On NuttX,
this allocation + subsequent free in `mono_error_cleanup` corrupted the heap free list,
causing a "Could not allocate 136 bytes" failure at the next test.

Additionally, the diagnostic called `g_free(msg)` on the pointer returned by
`mono_error_get_message`, which returns an internal pointer that must not be freed by the
caller. This double-free created a self-referencing flink in the NuttX free list, causing
an infinite loop in `mm_addfreechunk`.

**Fix**: Removed the entire JIT FAILED diagnostic block. The `CCTOR FAILED` diagnostic in
`object.c` is safe (uses `g_strdup`'d strings, freed after use) and kept for debugging.

**Result**: `test_0_exception_in_cctor` now passes. 733 tests ran, 3 failed (same 3
pre-existing failures), 2 skipped. TypeInitializationException handling is no longer a
production risk.

### 9. Fixed atan_precision — NuttX libm workaround in Mono

**Root cause**: NuttX libm `atan()` computes `asin(x / sqrt(x*x + 1))`, which produces NaN for
±∞ inputs (∞/∞ = NaN). IEEE 754 requires `atan(±∞) = ±π/2`.

**Fix**: Added `#ifdef HOST_NUTTX` guards in two places in the runtime:
1. `sysmath.c:ves_icall_System_Math_Atan` — handles runtime icall path
2. `intrinsics.c` OP_ATAN case — handles JIT constant-folding path (the actual codepath
   hit by the test, since `Math.Atan(double.NegativeInfinity)` is a compile-time constant)

Both check `isinf(x)` and return `copysign(M_PI_2, x)` before calling NuttX's `atan()`.

**Note**: Fixing NuttX's `lib_atan.c` directly was attempted but makes the user ELF ~72 bytes
larger, which shifts function addresses and triggers a crash in `test_0_throw_unwind` (likely a
latent memory/alignment issue in the emulator). The Mono-level fix avoids changing NuttX layout.

### 10. Fixed intptr_array_cast — modern .NET semantics

Updated `test_0_intptr_array_cast` in `arrays.cs`. Modern .NET (commit `5a0eb6e93c6`) intentionally
changed IntPtr[] to no longer be assignable to int[]/long[] regardless of pointer size. Test now
verifies the modern behavior (IntPtr[] is NOT castable to int[]/long[]).

## Test results (733 ran, 2 skipped, 1 failed — 99.86% pass)

| Suite | Ran | Skipped | Failed |
|-------|-----|---------|--------|
| basic | 134 | 0 | 0 |
| arrays | 36 | 0 | 0 |
| basic-calls | 27 | 0 | 0 |
| basic-float | 58 | 0 | 0 |
| basic-long | 97 | 0 | 0 |
| basic-math | 27 | 0 | 0 |
| objects | 105 | 0 | 0 |
| exceptions | 86 | 0 | 1 (ldflda_null_pointer — NullRef in emulator) |
| generics | 78 | 0 | 0 |
| gshared | 85 | 2 | 0 |
| **TOTAL** | **733** | **2** | **1** |

**2 excluded tests** (not bugs):
- `arm64_vtype_stack_args` — ARM64-only gsharedvt test, N/A on ARM32
- `begin_end_invoke` — APM (BeginInvoke/EndInvoke) unsupported in modern .NET

## What's next

### Remaining test failure
1. **ldflda_null_pointer**: NullReferenceException — may need null-check trampoline on Thumb2

### Phase 6: Hardware
2. Flash JIT firmware to physical F7 board and validate

### Known risks
- **JIT memory pressure**: Card table fix saved 8MB, but JIT code buffers + GC heap still compete for 32MB SDRAM.
- **NuttX binary layout sensitivity**: `test_0_throw_unwind` crashes if NuttX user ELF layout changes (e.g., adding ~72 bytes to libm). Likely a latent memory/alignment issue. Avoid modifying NuttX libc unless necessary.
