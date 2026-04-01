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
    /* Output the Mono code into SDRAM.                                       */
    /*                                                                        */
    /* .NET 10 Mono libraries placed in SDRAM (.mono section at 0xC0000000):  */
    /*  libmonosgen-2.0.a                     — main runtime                  */
    /*  libmono-component-marshal-ilgen-static.a — P/Invoke marshalling       */
    /*  libmono-component-hot_reload-stub-static.a                            */
    /*  libmono-component-debugger-stub-static.a                              */
    /*  libmono-component-diagnostics_tracing-stub-static.a                   */
    /*  libminipal.a                          — platform abstraction          */
    /*  libaotminipal.a                       — AOT platform abstraction      */
    /*  libsgen_objects.a                     — GC objects                    */
    /*                                                                        */

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
        *(EXCLUDE_FILE(*libmonosgen-2.0.a:* *libmono-component-marshal-ilgen-static.a:* *libmono-component-hot_reload-stub-static.a:* *libmono-component-debugger-stub-static.a:* *libmono-component-diagnostics_tracing-stub-static.a:* *libminipal.a:* *libaotminipal.a:* *libsgen_objects.a:*) .text .text.* .rodata .rodata.*)
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
        *(EXCLUDE_FILE(*libmonosgen-2.0.a:* *libmono-component-marshal-ilgen-static.a:* *libmono-component-hot_reload-stub-static.a:* *libmono-component-debugger-stub-static.a:* *libmono-component-diagnostics_tracing-stub-static.a:* *libminipal.a:* *libaotminipal.a:* *libsgen_objects.a:*) .ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = ABSOLUTE(.);
    } > uflash

    . = ALIGN(16);
    _eronly = ABSOLUTE(.);

    .data : {
        . = ALIGN(8192);
        _sdata = ABSOLUTE(.);
        *(EXCLUDE_FILE(*libmonosgen-2.0.a:* *libmono-component-marshal-ilgen-static.a:* *libmono-component-hot_reload-stub-static.a:* *libmono-component-debugger-stub-static.a:* *libmono-component-diagnostics_tracing-stub-static.a:* *libminipal.a:* *libaotminipal.a:* *libsgen_objects.a:*) .data .data.* .gnu.linkonce.d.* CONSTRUCTORS)
        _edata = ABSOLUTE(.);
    } > usram AT > uflash :data

    .bss : {
        . = ALIGN(512);
        _sbss = ABSOLUTE(.);
        *(EXCLUDE_FILE(*libmonosgen-2.0.a:* *libmono-component-marshal-ilgen-static.a:* *libmono-component-hot_reload-stub-static.a:* *libmono-component-debugger-stub-static.a:* *libmono-component-diagnostics_tracing-stub-static.a:* *libminipal.a:* *libaotminipal.a:* *libsgen_objects.a:*) .bss .bss.* .gnu.linkonce.b.* )
        . = ALIGN(8192);
        _ebss = ABSOLUTE(.);
    } > usram

    .comment 0 : {
        *(EXCLUDE_FILE(*libmonosgen-2.0.a:* *libmono-component-marshal-ilgen-static.a:* *libmono-component-hot_reload-stub-static.a:* *libmono-component-debugger-stub-static.a:* *libmono-component-diagnostics_tracing-stub-static.a:* *libminipal.a:* *libaotminipal.a:* *libsgen_objects.a:*)  .comment)
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
        LONG(###VERSION_BUILD###)
        LONG(###VERSION_REVISION###)
        LONG(###VERSION_MINOR###)
        LONG(###VERSION_MAJOR###)
        BYTE(###BUILD_DAY###)
        BYTE(###BUILD_MONTH###)
        BYTE(###BUILD_YEAR###)
        BYTE(###BUILD_HOUR###)
        BYTE(###BUILD_MINUTE###)
        BYTE(###BUILD_SECOND###)
        LONG(###BUILD_HASH###)
        ###MONO_GIT_REF###
        _e_mono_signature = ABSOLUTE(.);
    } > sdram :mono_text

    .mono : {
        _s_mono = ABSOLUTE(.);
        *libmonosgen-2.0.a:* (.text .text.*)
        *libmono-component-marshal-ilgen-static.a:* (.text .text.*)
        *libmono-component-hot_reload-stub-static.a:* (.text .text.*)
        *libmono-component-debugger-stub-static.a:* (.text .text.*)
        *libmono-component-diagnostics_tracing-stub-static.a:* (.text .text.*)
        *libminipal.a:* (.text .text.*)
        *libaotminipal.a:* (.text .text.*)
        *libsgen_objects.a:* (.text .text.*)
        _e_mono = ABSOLUTE(.);
    } > sdram

    .mono_data : {
        . = ALIGN(8192);
        _s_mono_data = ABSOLUTE(.);
        *libmonosgen-2.0.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmono-component-marshal-ilgen-static.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmono-component-hot_reload-stub-static.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmono-component-debugger-stub-static.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libmono-component-diagnostics_tracing-stub-static.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libminipal.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libaotminipal.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        *libsgen_objects.a:* (.data .data.* .rodata .rodata.* .gnu.linkonce.d.*)
        CONSTRUCTORS
        _e_mono_data = ABSOLUTE(.);
    } > sdram :mono_data

    .mono_comment : {
        . = ALIGN(1024);
        _s_mono_comment = ABSOLUTE(.);
        *libmonosgen-2.0.a:* (.comment)
        *libmono-component-marshal-ilgen-static.a:* (.comment)
        *libmono-component-hot_reload-stub-static.a:* (.comment)
        *libmono-component-debugger-stub-static.a:* (.comment)
        *libmono-component-diagnostics_tracing-stub-static.a:* (.comment)
        *libminipal.a:* (.comment)
        *libaotminipal.a:* (.comment)
        *libsgen_objects.a:* (.comment)
        _e_mono_comment = ABSOLUTE(.);
    } > sdram

    .mono_bss : {
        . = ALIGN(1024);
        _s_mono_bss = ABSOLUTE(.);
        *libmonosgen-2.0.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmono-component-marshal-ilgen-static.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmono-component-hot_reload-stub-static.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmono-component-debugger-stub-static.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libmono-component-diagnostics_tracing-stub-static.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libminipal.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libaotminipal.a:* (.bss .bss.* .gnu.linkonce.b.* )
        *libsgen_objects.a:* (.bss .bss.* .gnu.linkonce.b.* )
        . = ALIGN(1048576);
        _e_mono_bss = ABSOLUTE(.);
    } > sdram
}
