source Nuttx.py
source Nuttx_Tasks.py

set history save on
set history size unlimited
set history remove-duplicates unlimited
set history filename ~/.gdb_history

set output-radix 16
set mem inaccessible-by-default off

set confirm off
file ../nuttx/nuttx
add-symbol-file -readnow ../nuttx/nuttx
shell if test -f ../nuttx/configs/stm32f777zit6-meadow/kernel/nuttx_user.elf; then echo add-symbol-file -readnow ../nuttx/configs/stm32f777zit6-meadow/kernel/nuttx_user.elf; fi > /tmp/meadow_gdb
source /tmp/meadow_gdb
set confirm on

target remote :4242
set remote hardware-breakpoint-limit 8
set remote hardware-watchpoint-limit 4

