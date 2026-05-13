# Track 12 (AOT) — Bring-up Handoff

**Status as of 2026-05-13:** Cross-AOT pipeline lands end-to-end up to
`mono_aot_register_module` accepting both TinyTest and CoreLib. The next
blocker is mono execution of AOT'd CoreLib code (memfault in the runtime →
AOT transition). TinyTest AOT register works cleanly; the crash is
CoreLib-specific.

## What works today

1. **Cross-AOT compiler** built via `runtime/build.sh --subset Mono.AotCross
   --os osx --arch arm64 -c Release -p:MonoCrossAOTTargetOS=android`. The
   resulting `mono-aot-cross` at
   `runtime/artifacts/bin/mono/android.arm.Release/cross/android-arm/mono-aot-cross`
   emits real Thumb-2 (verified via `arm-none-eabi-objdump -M force-thumb`).
   Key fix: defining `__thumb2__` makes `arm-codegen.h` route through
   `thumb-codegen.h`. Currently done via a hard `#define` at the top of
   `arm-codegen.h` (committed as `eba6f256a4d` in the runtime repo).
   Long-term this should be driven by a proper AOT target triple
   (`thumbv7em-none-eabi-nuttx`) routed through `monoaotcross.proj`.

2. **AOT compile pipeline** (from `/tmp/aot-monobcl` working tree today):
   ```
   $AOT_CROSS \
     --aot=mtriple=armv7-none-linux-androideabi,asmonly,static \
     <assembly.dll>
   (echo .syntax unified; echo .thumb; cat <name>.dll.s) > <name>.thumb.s
   arm-none-eabi-gcc -mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 \
     -fPIC -shared -nostdlib -nostartfiles \
     -Wl,--unresolved-symbols=ignore-all \
     -o <name>.dll.so <name>.thumb.s
   ```
   The `static` flag is required to emit `mono_aot_module_<NAME>_info` as a
   per-module exported symbol (without it, the module exports the generic
   `mono_aot_file_info` and the runtime can't find the right struct).
   `asmonly` keeps the output as `.s` (we assemble + link separately so we
   can prepend `.syntax unified` + `.thumb` directives gas needs).

3. **On-device loader** (NuttX modlib via `dlopen`):
   - `nuttx/libs/libc/dlfcn/lib_dlopen.c` patched to handle `dlopen(NULL)`
     (POSIX self-handle) instead of dereferencing `file[0]`.
   - `apps/examples/mono/mono_nuttx_stubs.c` had catch-all `dl*` stubs
     intercepting everything → removed.
   - `apps/examples/mono/mono_main.c` scans `/meadow0/*.dll.so` after
     `monovm_initialize`, `dlopen`s each, `dlsym`s
     `mono_aot_module_<NAME>_info`, **dereferences the one-pointer
     indirection cell** (subtle: the symbol points to the struct, isn't
     the struct), and calls `mono_aot_register_module`.

4. **Runtime AOT loader compat** (`runtime/src/mono/...`):
   - `build-nuttx.sh`: `DISABLE_AOT=0` so libmonosgen has the loader code
     compiled in. Adds ~2MB to libmonosgen.
   - `aot-runtime.c get_call_table_entry()`: accept Thumb-2 `LDR.W PC, [PC, #0]`
     (`0xf000f8df`) trampoline encoding in addition to ARM-32
     `LDR PC, [PC, #-4]` (`0xe51ff004`). The instruction word isn't
     executed — only the target address from the second word — but the
     hardcoded ARM-32 assertion was failing for our Thumb-2 trampolines.
   - `sgen-cardtable.h`: gate the 26-bit `CARD_TABLE_BITS` path on either
     `HOST_NUTTX` (runtime) or `MEADOW_AOT_NUTTX_CARDS` (cross compiler)
     so both sides compute the same card-table mask. Mismatch was hitting
     `info->card_table_mask` assertion in `mono_aot_register_module`.

5. **Emulator support** (`Meadow.OS.Emulator`):
   - `tools/extract-hook-addresses.py` now auto-generates
     `mono_bss_zero.bin` (sized to the live `_s_mono_bss.._e_mono_bss` range
     from `nuttx_user.elf`) AND a `mono-bss.resc` `include`. After every
     mono lib rebuild the address + size stay in sync automatically.

6. **Verified state in the emulator:**
   ```
   AOT: registered mono_aot_module_System_Private_CoreLib_info (info=0xc0913e48, ver=187)
   AOT: registered mono_aot_module_TinyTest_info (info=0xc09302c0, ver=187)
   ```
   Both modules accepted (version, card-table, trampoline-format all
   match). TinyTest-only boot proceeds cleanly to managed-code entry.

## What's blocked

**MemManage fault ~370ms after Meadow.dll execution starts**, when mono
tries to invoke AOT'd CoreLib code. `MMFAR` shows a value that looks like
an unrelocated `.text` vaddr (e.g. `0x003bbde6` in one specific build,
varies per AOTID).

Investigation captured today:
- The value isn't a 4-byte-aligned literal anywhere in the .so file.
- The .o file has **no `R_ARM_ABS32` entries in `.rel.text`** — gas/Mono
  AOT emits `.long sym - . + addend` style PC-relative expressions, which
  become `R_ARM_REL32` (resolved at link time, valid at any load address).
- All R_ARM_ABS32 references are in `.rel.data.rel.ro`, which the linker
  correctly converts to `R_ARM_RELATIVE` in `.rel.dyn` → modlib applies
  these at load time.
- **So the codegen LOOKS PIC-correct** — yet we still crash.

Hypotheses for next session:
1. The runtime is double-relocating somewhere (adding load_base to a
   value that's already PC-relative-resolved).
2. A Mono AOT instruction pattern specific to Thumb-2 + NuttX that the
   on-device libmonosgen doesn't recognize.
3. Our forced `__thumb2__` define is making the cross compiler emit a
   slightly different opcode mix than the runtime expects.

Best next move: GDB session that breaks **before** the fault — set a
breakpoint at `mono_jit_compile_method_inner` or the trampoline that
transitions JIT → AOT, single-step through to the faulting instruction.
The crash bytes + the saved PC are reliable; we just need to know what
managed method is being invoked at that moment and what the AOT info
struct says about it.

## Tools & artifacts created this round

| Path | Purpose |
|------|---------|
| `aot-support/dl_smoke.c` | Tiny native module (one function) used to validate the dlopen → dlsym → call → dlclose path before introducing real AOT'd modules. |
| `aot-support/fixup-aot-relocs.py` | Post-link script: converts static `R_ARM_ABS32` in `.rel.text` into dynamic `R_ARM_RELATIVE` in `.rel.dyn` for modlib to apply. Works mechanically (ELF rewrite + shift + DT update). Currently a no-op for our codegen (.rel.text has only R_ARM_REL32) but kept for future codegen experiments. |
| Emulator `tools/extract-hook-addresses.py` `.mono_bss` block | Auto-gen the BSS zero blob + include script each emulator build. |

## Commits landed this round

```
Meadow:    f0be14acede  feat(aot): cross-AOT bring-up — dlopen + mono_aot_register_module path
Meadow:    c057d5de4ec  tools(aot): post-link fixup script to dynamic-ize text relocations
runtime:   eba6f256a4d  feat(nuttx): cross-AOT bring-up — Thumb2 codegen + Cortex-M7 compat fixes
emulator:  b3dab20      fix(emu): update .mono_bss range after AOT-enabled libmonosgen rebuild
emulator:  dbaaf1b      fix(emu): auto-generate .mono_bss range each build, stop manual patching
```

## Open dependencies on other agents/teams

* CLI agent has been asked (`agent-comms/os-to-cli.md`) to whitelist `.so`
  files in `meadow app run` deploy. Without this, Mono AOT modules must
  be pushed manually via `meadow file write` after each deploy.
