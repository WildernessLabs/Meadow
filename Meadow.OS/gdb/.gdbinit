source gdb/Nuttx.py
source gdb/Nuttx_Tasks.py

set output-radix 16
set mem inaccessible-by-default off

set confirm off
file nuttx
add-symbol-file -readnow nuttx
add-symbol-file -readnow configs/stm32f777zit6-meadow/kernel/nuttx_user.elf
set confirm on

target remote :4242
set remote hardware-breakpoint-limit 8
set remote hardware-watchpoint-limit 4

