set history save on
set history size unlimited
set history remove-duplicates unlimited
set history filename ~/.gdb_history

set output-radix 16
set mem inaccessible-by-default off

set remote hardware-breakpoint-limit 8
set remote hardware-watchpoint-limit 4

set confirm off

define load-nuttx-symbols
  file ../nuttx/nuttx.elf
  add-symbol-file -readnow ../nuttx/nuttx.elf
  #
  # The following loads the bootloader symbols for debugging but is commented out for general use
  # as it adds over 45 seconds to the debugger startup.
  #
  shell if test -f ../nuttx/nuttx_user.elf; then echo add-symbol-file -readnow ../nuttx/nuttx_user.elf; fi > /tmp/meadow_gdb
  source /tmp/meadow_gdb
end

#
# Read the bootloader symbols.
#
define read-bootloader-symbols
    add-symbol-file -readnow ../bootloader/Debug/Meadow.BL.elf
end

define reset-qemu
  load
  monitor system_reset
end

#
# Load the tensorflow symbols.
#
define load-tf-symbols
  add-symbol-file -readnow ../Tensorflow/Tensorflow.so
end

#
# Print the contents of the variable holding the RAMLOG (syslog).
#
define dmesg
  printf "%s", g_sysbuffer
end

#
#   These files are loaded after the NuttX symbol ffiles as references to
#   symbols are made in the files.  If they are loaded before the NuttX ELF
#   files then they will fail.
#
define enable_hardfault
  set *((uint32_t *) 0xe000edfc) |= 0x0000400
end

#
#   Start logging to file.
#
define start-file-logging
  set logging file gdblog.txt
  set logging on
  set trace-commands on
end

#
#   Stop logging to file.
#
define stop-file-logging
  set logging off
  set trace-commands off
end

load-nuttx-symbols
#
#   These files are loaded after the NuttX symbol ffiles as references to
#   symbols are made in the files.  If they are loaded before the NuttX ELF
#   files then they will fail.
#
source Nuttx.py
source Nuttx_Tasks.py
source NuttxHeap.py
source Tracing.py

target extended-remote :4242
mon gdb_breakpoint_override hard
#reset-qemu
monitor nuttx.pid_offset 12
monitor nuttx.xcpreg_offset 332
monitor nuttx.state_offset 26
monitor nuttx.name_offset 540
monitor nuttx.name_size 64
monitor nuttx.g_tasklisttable_size 72

#
#   Commands specific to your debug scenario should be placed here.  The following
#   may be useful for debugging sessions and so have been left as commants with
#   instructions for use / what they do.
#

#
#   Define a single macro that can be called from GDB to get to the point of
#   interest.  This is used by entering the command
#
#   run-to-error
#
#   at the GDB prompt.
#
# define run-to-error
#     mon reset halt
#     dis 2
#     c
#     en 2
#     c
#     c 7
# end

#
#   Change the colour used to show addresses.
#
# set style address foreground yellow

#
#   Set some breakpoints.
#
#   First a simple breakpoint:
#
# b hcom_nx_config_manager.c:2118
#
#   Now a more complex breakpoint.  This will stop at the specified line and then
#   show the current backtrace along with method arguments and local variables.
#
# b mm_malloc.c:138
# command
# nx_bt
# echo \nArguments\n\n
# info arg
# echo \nLocal Variables\n\n
# info local
# echo \nCurrent node\n\n
# p *node
# end
# dis 2

# b hcom_misc_rqst_get_device_info

#
#   Turn pagination off - useful when running some of the heap tracing macros
#   and aso may be useful for use with trace files.
#
# set pagination off

#
#   Send the commands to the trace file.  Useful to show the command and the
#   result in the trace file.  Without this you will only get the command result
#   in the file.
#
# set trace-commands on

#
#   Turn logging to file on.  By default the log file will be placed in the
#   same directory as the gdbinit file.
#
# set logging file gdblog.txt
# set logging on
# set trace-commands on

#
#   Show all the breakpoints and their status (along with any commands associated
#   with the breakpoint) to check current status.
#
# info b
