source Nuttx.py
source Nuttx_Tasks.py

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
  shell if test -f ../nuttx/nuttx_user.elf; then echo add-symbol-file -readnow ../nuttx/nuttx_user.elf; fi > /tmp/meadow_gdb
  source /tmp/meadow_gdb
end

define reset-qemu
  load
  monitor system_reset
end


load-nuttx-symbols
target remote :4242
mon gdb_breakpoint_override hard
#reset-qemu
