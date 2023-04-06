target remote :3333

mon reset halt
flushregs

thb heap_trace_start
commands
mon esp32 sysview start file://heap0_log.svdat file://heap1_log.svdat
c
end

thb heap_trace_stop
commands
mon esp32 sysview stop
set confirm off
quit
end

c
