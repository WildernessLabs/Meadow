#!/usr/bin/env python3
"""Post-link fixup for Mono AOT .so files cross-compiled for Meadow.

Background
----------
Mono AOT emits cross-method pointer literals as `.long .Lmethod_xx` inside
.text. Gas (the GNU assembler) emits R_ARM_ABS32 relocations for these in
the .o file's .rel.text section. When linking with `-shared -fPIC`, ld
RESOLVES these to absolute link-time vaddrs (e.g. 0x003bbde6) and embeds
the value in .text — it does NOT emit R_ARM_RELATIVE entries in .rel.dyn.

For an embedded shared object that gets `dlopen`'d at an arbitrary load
address (as Mono AOT modules do under NuttX/modlib), this is broken: the
literals stay frozen at link-time vaddrs, and when AOT'd code dereferences
them it faults on unmapped low memory.

What this tool does
-------------------
Takes a .so built with `-Wl,-q` (--emit-relocs), which preserves the
static R_ARM_ABS32 entries in `.rel.text`. Converts each to a dynamic
R_ARM_RELATIVE entry and appends to `.rel.dyn`. The dynamic loader will
then add `load_base` to each literal slot at load time — exactly what
we'd want from a properly-PIC .text section.

Mechanically: extend the `.rel.dyn` section in place, shift all later
sections' file offsets forward by the size delta, update DT_RELSZ +
DT_RELCOUNT in .dynamic.

Usage
-----
    fixup-aot-relocs.py <input.so> [output.so]
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.enums import ENUM_RELOC_TYPE_ARM

R_ARM_ABS32 = ENUM_RELOC_TYPE_ARM["R_ARM_ABS32"]      # 2
R_ARM_RELATIVE = ENUM_RELOC_TYPE_ARM["R_ARM_RELATIVE"]  # 23

DT_REL = 17
DT_RELSZ = 18
DT_RELENT = 19
DT_RELCOUNT = 0x6ffffffa


def fixup(infile: Path, outfile: Path) -> None:
    raw = bytearray(infile.read_bytes())

    with infile.open("rb") as f:
        elf = ELFFile(f)

        # Locate sections we care about
        rel_text = elf.get_section_by_name(".rel.text")
        rel_dyn = elf.get_section_by_name(".rel.dyn")
        dynamic = elf.get_section_by_name(".dynamic")
        text = elf.get_section_by_name(".text")

        if rel_text is None:
            sys.exit("ERROR: .rel.text not found — was the .so linked with -Wl,-q?")
        if rel_dyn is None:
            sys.exit("ERROR: .rel.dyn not found")
        if dynamic is None:
            sys.exit("ERROR: .dynamic not found")
        if text is None:
            sys.exit("ERROR: .text not found")

        text_start = text["sh_addr"]
        text_end = text["sh_addr"] + text["sh_size"]

        # Collect candidate new relocations from .rel.text
        new_relocs: list[int] = []   # list of r_offset values
        for rel in rel_text.iter_relocations():
            r_offset = rel["r_offset"]
            r_type = rel["r_info_type"]
            if r_type != R_ARM_ABS32:
                continue
            if not (text_start <= r_offset < text_end):
                continue
            new_relocs.append(r_offset)

        n_new = len(new_relocs)
        if n_new == 0:
            print("No new relocations to add — nothing to do.")
            outfile.write_bytes(raw)
            return

        rel_dyn_off = rel_dyn["sh_offset"]
        rel_dyn_sz = rel_dyn["sh_size"]
        relent = rel_dyn["sh_entsize"]
        assert relent == 8, f"unexpected RELENT={relent}"

        # Capture existing .rel.dyn count for DT_RELCOUNT update
        old_count = rel_dyn_sz // relent

        # Build the new entries blob (8 bytes per: r_offset, r_info)
        new_blob = b"".join(
            struct.pack("<II", off, (0 << 8) | R_ARM_RELATIVE)
            for off in new_relocs
        )
        delta = len(new_blob)

        # Patch logic: insert new_blob right after the existing .rel.dyn,
        # then shift everything from (rel_dyn_off + rel_dyn_sz) onwards by
        # `delta` bytes. Update file-offset references in section / program
        # headers / dynamic entries to reflect the shift.

        insert_point = rel_dyn_off + rel_dyn_sz
        before = bytes(raw[:insert_point])
        after = bytes(raw[insert_point:])
        raw = bytearray(before + new_blob + after)

        # Helper: shift any file-offset > insert_point by `delta`.
        def shift_off(off: int) -> int:
            return off + delta if off > insert_point else off

        # --- ELF header: e_phoff, e_shoff ---
        # ELF32 header layout: ei_magic(16), e_type(2), e_machine(2), e_version(4),
        #   e_entry(4), e_phoff(4), e_shoff(4), e_flags(4), e_ehsize(2),
        #   e_phentsize(2), e_phnum(2), e_shentsize(2), e_shnum(2), e_shstrndx(2)
        ehsize = elf["e_ehsize"]
        e_phoff_raw, = struct.unpack_from("<I", raw, 28)
        e_shoff_raw, = struct.unpack_from("<I", raw, 32)
        struct.pack_into("<I", raw, 28, shift_off(e_phoff_raw))
        struct.pack_into("<I", raw, 32, shift_off(e_shoff_raw))

        # --- Program headers: p_offset (each 32 bytes for ELF32) ---
        phoff_new = shift_off(e_phoff_raw)
        phentsize = elf["e_phentsize"]
        phnum = elf["e_phnum"]
        for i in range(phnum):
            phdr_off = phoff_new + i * phentsize
            p_offset, = struct.unpack_from("<I", raw, phdr_off + 4)
            new_p_offset = shift_off(p_offset)
            struct.pack_into("<I", raw, phdr_off + 4, new_p_offset)
            # Adjust p_filesz / p_memsz if this segment CONTAINS the insertion
            p_filesz, = struct.unpack_from("<I", raw, phdr_off + 16)
            p_memsz, = struct.unpack_from("<I", raw, phdr_off + 20)
            seg_end = p_offset + p_filesz
            if p_offset <= insert_point < seg_end:
                # Insertion is inside this segment — grow it
                struct.pack_into("<I", raw, phdr_off + 16, p_filesz + delta)
                struct.pack_into("<I", raw, phdr_off + 20, p_memsz + delta)

        # --- Section headers ---
        shoff_new = shift_off(e_shoff_raw)
        shentsize = elf["e_shentsize"]
        shnum = elf["e_shnum"]
        for i in range(shnum):
            shdr_off = shoff_new + i * shentsize
            sh_offset, = struct.unpack_from("<I", raw, shdr_off + 16)
            sh_size, = struct.unpack_from("<I", raw, shdr_off + 20)
            if sh_offset == rel_dyn_off:
                # This IS .rel.dyn — grow it in place
                struct.pack_into("<I", raw, shdr_off + 20, sh_size + delta)
            else:
                struct.pack_into("<I", raw, shdr_off + 16, shift_off(sh_offset))

        # --- .dynamic entries: DT_RELSZ, DT_RELCOUNT ---
        # The .dynamic section vaddr/offset itself may have shifted; re-read.
        dyn_shoff = None
        for i in range(shnum):
            shdr_off = shoff_new + i * shentsize
            sh_type, = struct.unpack_from("<I", raw, shdr_off + 4)
            if sh_type == 6:  # SHT_DYNAMIC
                dyn_shoff, = struct.unpack_from("<I", raw, shdr_off + 16)
                dyn_size, = struct.unpack_from("<I", raw, shdr_off + 20)
                break
        if dyn_shoff is None:
            sys.exit("ERROR: SHT_DYNAMIC section header not found after shift")

        # Walk dynamic entries (each is 8 bytes: d_tag + d_val/d_ptr)
        for i in range(dyn_size // 8):
            ent_off = dyn_shoff + i * 8
            d_tag, d_val = struct.unpack_from("<II", raw, ent_off)
            if d_tag == 0:  # DT_NULL — end of table
                break
            if d_tag == DT_RELSZ:
                struct.pack_into("<I", raw, ent_off + 4, d_val + delta)
            elif d_tag == DT_RELCOUNT:
                struct.pack_into("<I", raw, ent_off + 4, d_val + n_new)
            elif d_tag == DT_REL:
                # Address of .rel.dyn — does NOT change (we extended in
                # place, the section's vaddr/file-offset is unchanged).
                pass

        outfile.write_bytes(bytes(raw))
        print(f"Wrote {outfile}: +{n_new} R_ARM_RELATIVE entries "
              f"(.rel.dyn: {old_count} -> {old_count + n_new})")


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit("Usage: fixup-aot-relocs.py <input.so> [output.so]")
    infile = Path(sys.argv[1])
    outfile = Path(sys.argv[2] if len(sys.argv) >= 3 else sys.argv[1])
    fixup(infile, outfile)


if __name__ == "__main__":
    main()
