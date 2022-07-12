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
target extended-remote :4242
eval "monitor nuttx.pid_offset %d", &((struct tcb_s *)(0))->pid
eval "monitor nuttx.xcpreg_offset %d", &((struct tcb_s *)(0))->xcp.regs
eval "monitor nuttx.state_offset %d", &((struct tcb_s *)(0))->task_state
eval "monitor nuttx.name_offset %d", &((struct tcb_s *)(0))->name
eval "monitor nuttx.name_size %d", sizeof(((struct tcb_s *)(0))->name)
mon gdb_breakpoint_override hard
#reset-qemu

monitor nuttx.g_tasklisttable_size 72

define armex
  printf "EXEC_RETURN (LR):\n",
  info registers $lr
    if ($lr & (0x4 == 0x4))
      printf "Uses MSP 0x%x return.\n", $msp
      set $armex_base = $psp
    else
      printf "Uses PSP 0x%x return.\n", $psp
      set $armex_base = $psp
    end
    printf "xPSR            0x%x\n", *(((uint32_t*)$armex_base)+7)
    printf "ReturnAddress   0x%x\n", *(((uint32_t*)$armex_base)+6)
    printf "LR (R14)        0x%x\n", *(((uint32_t*)$armex_base)+5)
    printf "R12             0x%x\n", *(((uint32_t*)$armex_base)+4)
    printf "R3              0x%x\n", *(((uint32_t*)$armex_base)+3)
    printf "R2              0x%x\n", *(((uint32_t*)$armex_base)+2)
    printf "R1              0x%x\n", *(((uint32_t*)$armex_base)+1)
    printf "R0              0x%x\n", *((uint32_t*)$armex_base)
    printf "Return instruction:\n"
    x/i *((uint32_t*)$armex_base+6)
    printf "LR instruction:\n"
    x/i *((uint32_t*)$armex_base+5)

    printf "SP 0x%x\n", *((uint32_t*)$r4)
    printf "EXC_RETURN 0x%x\n", *((uint32_t*)$r4+10)
end
  
document armex
ARMv7 Exception entry behavior.
xPSR, ReturnAddress, LR (R14), R12, R3, R2, R1, and R0
end

break up_assert