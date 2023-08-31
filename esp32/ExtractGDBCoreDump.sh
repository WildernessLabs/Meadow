#!/bin/bash

if test "$#" -ne 1; then
    echo "usage: $0 serial_port_name"
    echo "serial_port_name should be the serial port connected to the ESP serial port."
    exit 1  
fi

ELF_FILE="build/MeadowComms.elf"
if test -f "ELF_FILE"; then
    espcoredump.py -p $1 dbg_corefile $ELF_FILE
else
    echo "Cannot locate ELF file."
fi