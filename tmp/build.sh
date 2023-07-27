#!/bin/bash

set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

arm-none-eabi-gcc -mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 -fPIC -fPIE -c -o test.o test.c
arm-none-eabi-g++ -mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 -fPIC -fPIE -c -o test1.o test1.cxx
arm-none-eabi-ld -Bsymbolic -G -Bdynamic -lsupc++ -lstdc++ -o test.so test.o test1.o