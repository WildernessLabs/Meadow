/****************************************************************************
 * configs/stm32f777zit6-meadow/scripts/user-space.ld
 *
 *   Copyright (C) 2015 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/* NOTE:  This depends on the memory.ld script having been included prior to
 * this script.
 */

OUTPUT_ARCH(arm)
ENTRY(_stext)

PHDRS
{
  text PT_LOAD;
  data PT_LOAD;
  comment PT_LOAD;
  mono_text PT_LOAD;
  mono_data PT_LOAD;
  mono_comment PT_LOAD;
}

SECTIONS
{
    /* Output the Mono code into the first 2MB of QSPI flash.                   */
    /*                                                                          */
    /* For this we need to list all the Mono object files to the linker script  */
    /* The objects are obtained from these files by using `arm-none-eabi-nm`:   */
    /*                                                                          */
    /*  libeglib.a                                                              */
    /*  libmini.a                                                               */
    /*  libmono-dbg.a                                                           */
    /*  libmono-ee-interp.a                                                     */
    /*  libmono-native.a                                                        */
    /*  libmonodis.a                                                            */
    /*  libmonomath.a                                                           */
    /*  libmonoruntime-config.a                                                 */
    /*  libmonoruntime-support.a                                                */
    /*  libmonoruntimesgen.a                                                    */
    /*  libmonosgen.a                                                           */
    /*  libmonoutils.a                                                          */
    /*                                                                          */

    /* Remove information from the Mono libraries */
    /DISCARD/ :
    {
        /* workaround relocation R_ARM_PREL31 out of range: 3087872792 is not in [-1073741824, 1073741823] */
        *(.ARM.exidx)
    }

    .userspace : {
        _stext = ABSOLUTE(.);
        . += 512;
        _suserspace = ABSOLUTE(.);
        /* KEEP is needed in case --gc-sections is being used */
        KEEP(*(.userspace))
        _euserspace = ABSOLUTE(.);
    } > uflash :text

    .text : {
        *(EXCLUDE_FILE(*libmonosgen.a:* *libmini.a:* *libmono-dbg.a:* *libmono-ee-interp.a:* *libeglib.a:* *libmonoruntime-config.a:* *libmonoruntimesgen.a:* *libmonoutils.a:* *libmonodis.a:* *libmono-native.a:* *libmonoruntime-support.a:* *libz.a:*) .text .text.* .rodata .rodata.*)
        . = ALIGN(131072);
        _etext = ABSOLUTE(.);
    } > uflash

    .init_section : {
        _sinit = ABSOLUTE(.);
        *(.init_array .init_array.*)
        _einit = ABSOLUTE(.);
    } > uflash

    .got : { 
	_got = .;
	*(.got) 
	*(.got.*) 
	_egot = .;
    } > uflash

    .ARM.extab : {
        *(.ARM.extab*)
    } > uflash

    .ARM.exidx : {
        __exidx_start = ABSOLUTE(.);
        *(EXCLUDE_FILE(*libmonosgen.a:* *libmini.a:* *libmono-dbg.a:* *libmono-ee-interp.a:* *libeglib.a:* *libmonoruntime-config.a:* *libmonoruntimesgen.a:* *libmonoutils.a:* *libmonodis.a:* *libmono-native.a:* *libmonoruntime-support.a:* *libz.a:*) .ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = ABSOLUTE(.);
    } > uflash

    . = ALIGN(16);
    _eronly = ABSOLUTE(.);

    .data : {
        . = ALIGN(8192);
        _sdata = ABSOLUTE(.);
        *(EXCLUDE_FILE(*libmonosgen.a:* *libmini.a:* *libmono-dbg.a:* *libmono-ee-interp.a:* *libeglib.a:* *libmonoruntime-config.a:* *libmonoruntimesgen.a:* *libmonoutils.a:* *libmonodis.a:* *libmono-native.a:* *libmonoruntime-support.a:* *libz.a:*) .data .data.* .gnu.linkonce.d.* CONSTRUCTORS)
        _edata = ABSOLUTE(.);
    } > usram AT > uflash :data

    .bss : {
        . = ALIGN(512);
        _sbss = ABSOLUTE(.);
        *(EXCLUDE_FILE(*libmonosgen.a:* *libmini.a:* *libmono-dbg.a:* *libmono-ee-interp.a:* *libeglib.a:* *libmonoruntime-config.a:* *libmonoruntimesgen.a:* *libmonoutils.a:* *libmonodis.a:* *libmono-native.a:* *libmonoruntime-support.a:* *libz.a:*) .bss .bss.* .gnu.linkonce.b.* )
        . = ALIGN(8192);
        _ebss = ABSOLUTE(.);
    } > usram

    .comment 0 : { 
        *(EXCLUDE_FILE(*libmonosgen.a:* *libmini.a:* *libmono-dbg.a:* *libmono-ee-interp.a:* *libeglib.a:* *libmonoruntime-config.a:* *libmonoruntimesgen.a:* *libmonoutils.a:* *libmonodis.a:* *libmono-native.a:* *libmonoruntime-support.a:* *libz.a:*)  .comment)
    } > usram :comment

    /* Stabs debugging sections */

    .stab 0 : { *(.stab) }
    .stabstr 0 : { *(.stabstr) }
    .stab.excl 0 : { *(.stab.excl) }
    .stab.exclstr 0 : { *(.stab.exclstr) }
    .stab.index 0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }

    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_info 0 : { *(.debug_info) }
    .debug_line 0 : { *(.debug_line) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_aranges 0 : { *(.debug_aranges) }

    .mono_signature : {
        _s_mono_signature = ABSOLUTE(.);
        FILL(0xDEADC0DE)
        BYTE(0xAA)
        BYTE(0xBB)
        BYTE(0xCC)
        BYTE(0xDD)
        // meadow OS version
        BYTE(17)
        BYTE(7)
        BYTE(6)
        BYTE(0)
        . += 16;
        _e_mono_signature = ABSOLUTE(.);
    } > sdram :mono_text

    .mono : {
        _s_mono = ABSOLUTE(.);
        *libmonosgen.a:* (.text .text.*)
        *libmini.a:* (.text .text.*)
        *libmono-dbg.a:* (.text .text.*)
        *libmono-ee-interp.a:* (.text .text.*)
        *libeglib.a:* (.text .text.*)
        *libmonoruntime-config.a:* (.text .text.*)
        *libmonoruntimesgen.a:* (.text .text.*)
        *libmonoutils.a:* (.text .text.*)
        *libmonodis.a:* (.text .text.*)
        *libmono-native.a:* (.text .text.*)
        *libmonoruntime-support.a:* (.text .text.*)
        *libz.a:* (.text .text.*)
        _e_mono = ABSOLUTE(.);
    } > sdram

    .mono_data : {
        . = ALIGN(8192);
        _s_mono_data = ABSOLUTE(.);
        *libmonosgen.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmini.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmono-dbg.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmono-ee-interp.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libeglib.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmonoruntime-config.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmonoruntimesgen.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmonoutils.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmonodis.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmono-native.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmonoruntime-support.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libz.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        CONSTRUCTORS
        _e_mono_data = ABSOLUTE(.);
    } > sdram :mono_data

    .mono_comment : {
        . = ALIGN(1024);
        _s_mono_comment = ABSOLUTE(.);
        *libmonosgen.a:* (.comment)
        *libmini.a:* (.comment)
        *libmono-dbg.a:* (.comment)
        *libmono-ee-interp.a:* (.comment)
        *libeglib.a:* (.comment)
        *libmonoruntime-config.a:* (.comment)
        *libmonoruntimesgen.a:* (.comment)
        *libmonoutils.a:* (.comment)
        *libmonodis.a:* (.comment)
        *libmono-native.a:* (.comment)
        *libmonoruntime-support.a:* (.comment)
        *libz.a:* (.comment)
        _e_mono_comment = ABSOLUTE(.);
    } > sdram

    .mono_bss : {
        . = ALIGN(1024);
        _s_mono_bss = ABSOLUTE(.);
        *libmonosgen.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmini.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmono-dbg.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmono-ee-interp.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libeglib.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmonoruntime-config.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmonoruntimesgen.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmonoutils.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmonodis.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmono-native.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmonoruntime-support.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libz.a:* (.bss .bss.* .gnu.linkonce.b.* )
        . = ALIGN(1048576);
        _e_mono_bss = ABSOLUTE(.);
    } > sdram
}
