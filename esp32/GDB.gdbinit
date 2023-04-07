define hook-quit
    set confirm off
end

target remote :3333 

mon reset halt

#mon esp32 appimage_offset 0x10000
#mon reset halt

flushregs
set remote hardware-watchpoint-limit 2
set listsize 30

thb app_main
c