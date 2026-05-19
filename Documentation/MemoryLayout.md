# Meadow OS Memory Layout

This document describes the memory layout of the Meadow OS application as defined by the linker scripts in the project. The layout is primarily determined by the linker script files found in the `nuttx/configs/stm32f777zit6-meadow/scripts/` directory, such as `kernel-space.ld`.

## Overview

The memory layout is designed for the STM32F777ZIT6 microcontroller, which features several memory regions:

- **FLASH**: Non-volatile memory for storing code and read-only data.
- **SRAM1**: Main static RAM for data, stack, and heap.
- **SRAM2**: Additional static RAM.
- **CCMRAM**: Core-coupled memory for fast access.
- **PERIPHERALS**: Memory-mapped I/O for device registers.

## Typical Memory Regions

| Region       | Start Address | Size      | Purpose                        |
|--------------|--------------|-----------|--------------------------------|
| FLASH        | 0x08000000    | 2MB       | Application code, constants    |
| SRAM1        | 0x20020000    | 384KB     | Data, stack, heap              |
| SRAM2        | 0x2007C000    | 128KB     | Additional data                |
| CCMRAM       | 0x10000000    | 64KB      | Fast-access RAM                |
| PERIPHERALS  | 0x40000000    | 512MB     | Device registers               |

## Linker Script Sections

The linker scripts define how the application is mapped into these regions. Key sections include:

- **.isr_vector**: Interrupt vector table, placed at the start of FLASH.
- **.text**: Executable code, stored in FLASH.
- **.rodata**: Read-only data, stored in FLASH.
- **.data**: Initialized data, loaded into SRAM1 at startup.
- **.bss**: Uninitialized data, zeroed in SRAM1 at startup.
- **.stack**: Stack space, allocated in SRAM1.
- **.heap**: Heap space, allocated in SRAM1 after .bss and .data.

## Example Section Placement

```
FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 2048K
SRAM1 (xrw)     : ORIGIN = 0x20020000, LENGTH = 384K
SRAM2 (xrw)     : ORIGIN = 0x2007C000, LENGTH = 128K
CCMRAM (xrw)    : ORIGIN = 0x10000000, LENGTH = 64K
PERIPHERALS (rw): ORIGIN = 0x40000000, LENGTH = 512K

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
  } > FLASH

  .data : AT (ADDR(.text) + SIZEOF(.text))
  {
    *(.data*)
  } > SRAM1

  .bss :
  {
    *(.bss*)
  } > SRAM1

  .stack :
  {
    *(.stack*)
  } > SRAM1
}
```

## Notes

- The **.isr_vector** section is always placed at the start of FLASH to ensure correct interrupt handling.
- The **.data** section is copied from FLASH to SRAM1 at startup.
- The **.bss** section is zero-initialized in SRAM1.
- The **.heap** and **.stack** grow towards each other in SRAM1.
- CCMRAM can be used for performance-critical data.

## References
- [STM32F777ZIT6 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00318631.pdf)
- Linker scripts in `nuttx/configs/stm32f777zit6-meadow/scripts/`

---

This layout ensures efficient use of the STM32F777ZIT6's memory resources for Meadow OS applications.