# Exception/interrupt vector numbers ******************************************************

                                    # Vector  0: Reset stack pointer value
                                    # Vector  1: Reset
IRQ_NMI                   = (2)     # Vector  2: Non-Maskable Interrupt= (NMI)
IRQ_HARDFAULT             = (3)     # Vector  3: Hard fault
IRQ_MEMFAULT              = (4)     # Vector  4: Memory management= (MPU)
IRQ_BUSFAULT              = (5)     # Vector  5: Bus fault
IRQ_USAGEFAULT            = (6)     # Vector  6: Usage fault
                                    # Vectors 7-10: Reserved
IRQ_SVCALL                = (11)    # Vector 11: SVC call
IRQ_DBGMONITOR            = (12)    # Vector 12: Debug Monitor
                                    # Vector 13: Reserved
IRQ_PENDSV                = (14)    # Vector 14: Pendable system service request
IRQ_SYSTICK               = (15)    # Vector 15: System tick

# External interrupts= (vectors >= 16).  These definitions are chip-specific

IRQ_FIRST                 = (16)    # Vector number of the first interrupt

# NVIC base address ***********************************************************************

ARMV7M_NVIC_BASE          = 0xe000e000

# NVIC register offsets *******************************************************************

ICTR_OFFSET               = 0x0004  # Interrupt controller type register
SYSTICK_CTRL_OFFSET       = 0x0010  # SysTick control and status register
SYSTICK_RELOAD_OFFSET     = 0x0014  # SysTick reload value register
SYSTICK_CURRENT_OFFSET    = 0x0018  # SysTick current value register
SYSTICK_CALIB_OFFSET      = 0x001c  # SysTick calibration value register

IRQ0_31_ENABLE_OFFSET     = 0x0100  # IRQ 0-31 set enable register
IRQ32_63_ENABLE_OFFSET    = 0x0104  # IRQ 32-63 set enable register
IRQ64_95_ENABLE_OFFSET    = 0x0108  # IRQ 64-95 set enable register
IRQ96_127_ENABLE_OFFSET   = 0x010c  # IRQ 96-127 set enable register
IRQ128_159_ENABLE_OFFSET  = 0x0110  # IRQ 128-159 set enable register
IRQ160_191_ENABLE_OFFSET  = 0x0114  # IRQ 160-191 set enable register
IRQ192_223_ENABLE_OFFSET  = 0x0118  # IRQ 192-223 set enable register
IRQ224_239_ENABLE_OFFSET  = 0x011c  # IRQ 224-239 set enable register

IRQ0_31_CLEAR_OFFSET      = 0x0180  # IRQ 0-31 clear enable register
IRQ32_63_CLEAR_OFFSET     = 0x0184  # IRQ 32-63 clear enable register
IRQ64_95_CLEAR_OFFSET     = 0x0188  # IRQ 64-95 clear enable register
IRQ96_127_CLEAR_OFFSET    = 0x018c  # IRQ 96-127 clear enable register
IRQ128_159_CLEAR_OFFSET   = 0x0190  # IRQ 128-159 clear enable register
IRQ160_191_CLEAR_OFFSET   = 0x0194  # IRQ 160-191 clear enable register
IRQ192_223_CLEAR_OFFSET   = 0x0198  # IRQ 192-223 clear enable register
IRQ224_239_CLEAR_OFFSET   = 0x019c  # IRQ 224-2391 clear enable register

IRQ0_31_PEND_OFFSET       = 0x0200  # IRQ 0-31 set pending register
IRQ32_63_PEND_OFFSET      = 0x0204  # IRQ 32-63 set pending register
IRQ64_95_PEND_OFFSET      = 0x0208  # IRQ 64-95 set pending register
IRQ96_127_PEND_OFFSET     = 0x020c  # IRQ 96-127 set pending register
IRQ128_159_PEND_OFFSET    = 0x0210  # IRQ 128-159 set pending register
IRQ160_191_PEND_OFFSET    = 0x0214  # IRQ 160-191 set pending register
IRQ192_223_PEND_OFFSET    = 0x0218  # IRQ 192-2231 set pending register
IRQ224_239_PEND_OFFSET    = 0x021c  # IRQ 224-2391 set pending register

IRQ0_31_CLRPEND_OFFSET    = 0x0280  # IRQ 0-31 clear pending register
IRQ32_63_CLRPEND_OFFSET   = 0x0284  # IRQ 32-63 clear pending register
IRQ64_95_CLRPEND_OFFSET   = 0x0288  # IRQ 64-95 clear pending register
IRQ96_127_CLRPEND_OFFSET  = 0x028c  # IRQ 96-127 clear pending register
IRQ128_159_CLRPEND_OFFSET = 0x0290  # IRQ 128-159 clear pending register
IRQ160_191_CLRPEND_OFFSET = 0x0294  # IRQ 160-191 clear pending register
IRQ192_223_CLRPEND_OFFSET = 0x0298  # IRQ 192-223 clear pending register
IRQ224_239_CLRPEND_OFFSET = 0x029c  # IRQ 224-239 clear pending register

IRQ0_31_ACTIVE_OFFSET     = 0x0300  # IRQ 0-31 active bit register
IRQ32_63_ACTIVE_OFFSET    = 0x0304  # IRQ 32-63 active bit register
IRQ64_95_ACTIVE_OFFSET    = 0x0308  # IRQ 64-95 active bit register
IRQ96_127_ACTIVE_OFFSET   = 0x030c  # IRQ 96-127 active bit register
IRQ128_159_ACTIVE_OFFSET  = 0x0310  # IRQ 128-159 active bit register
IRQ160_191_ACTIVE_OFFSET  = 0x0314  # IRQ 160-191 active bit register
IRQ192_223_ACTIVE_OFFSET  = 0x0318  # IRQ 192-223 active bit register
IRQ224_239_ACTIVE_OFFSET  = 0x031c  # IRQ 224-239 active bit register

IRQ0_3_PRIORITY_OFFSET    = 0x0400  # IRQ 0-3 priority register
IRQ4_7_PRIORITY_OFFSET    = 0x0404  # IRQ 4-7 priority register
IRQ8_11_PRIORITY_OFFSET   = 0x0408  # IRQ 8-11 priority register
IRQ12_15_PRIORITY_OFFSET  = 0x040c  # IRQ 12-15 priority register
IRQ16_19_PRIORITY_OFFSET  = 0x0410  # IRQ 16-19 priority register
IRQ20_23_PRIORITY_OFFSET  = 0x0414  # IRQ 20-23 priority register
IRQ24_27_PRIORITY_OFFSET  = 0x0418  # IRQ 24-29 priority register
IRQ28_31_PRIORITY_OFFSET  = 0x041c  # IRQ 28-31 priority register
IRQ32_35_PRIORITY_OFFSET  = 0x0420  # IRQ 32-35 priority register
IRQ36_39_PRIORITY_OFFSET  = 0x0424  # IRQ 36-39 priority register
IRQ40_43_PRIORITY_OFFSET  = 0x0428  # IRQ 40-43 priority register
IRQ44_47_PRIORITY_OFFSET  = 0x042c  # IRQ 44-47 priority register
IRQ48_51_PRIORITY_OFFSET  = 0x0430  # IRQ 48-51 priority register
IRQ52_55_PRIORITY_OFFSET  = 0x0434  # IRQ 52-55 priority register
IRQ56_59_PRIORITY_OFFSET  = 0x0438  # IRQ 56-59 priority register
IRQ60_63_PRIORITY_OFFSET  = 0x043c  # IRQ 60-63 priority register
IRQ64_67_PRIORITY_OFFSET  = 0x0440  # IRQ 64-67 priority register
IRQ68_71_PRIORITY_OFFSET  = 0x0444  # IRQ 68-71 priority register
IRQ72_75_PRIORITY_OFFSET  = 0x0448  # IRQ 72-75 priority register
IRQ76_79_PRIORITY_OFFSET  = 0x044c  # IRQ 76-79 priority register
IRQ80_83_PRIORITY_OFFSET  = 0x0450  # IRQ 80-83 priority register
IRQ84_87_PRIORITY_OFFSET  = 0x0454  # IRQ 84-87 priority register
IRQ88_91_PRIORITY_OFFSET  = 0x0458  # IRQ 88-91 priority register
IRQ92_95_PRIORITY_OFFSET  = 0x045c  # IRQ 92-95 priority register
IRQ96_99_PRIORITY_OFFSET  = 0x0460  # IRQ 96-99 priority register
IRQ100_103_PRIORITY_OFFSET= 0x0464  # IRQ 100-103 priority register
IRQ104_107_PRIORITY_OFFSET= 0x0468  # IRQ 104-107 priority register
IRQ108_111_PRIORITY_OFFSET= 0x046c  # IRQ 108-111 priority register
IRQ112_115_PRIORITY_OFFSET= 0x0470  # IRQ 112-115 priority register
IRQ116_119_PRIORITY_OFFSET= 0x0474  # IRQ 116-119 priority register
IRQ120_123_PRIORITY_OFFSET= 0x0478  # IRQ 120-123 priority register
IRQ124_127_PRIORITY_OFFSET= 0x047c  # IRQ 124-127 priority register
IRQ128_131_PRIORITY_OFFSET= 0x0480  # IRQ 128-131 priority register
IRQ132_135_PRIORITY_OFFSET= 0x0484  # IRQ 132-135 priority register
IRQ136_139_PRIORITY_OFFSET= 0x0488  # IRQ 136-139 priority register
IRQ140_143_PRIORITY_OFFSET= 0x048c  # IRQ 140-143 priority register
IRQ144_147_PRIORITY_OFFSET= 0x0490  # IRQ 144-147 priority register
IRQ148_151_PRIORITY_OFFSET= 0x0494  # IRQ 148-151 priority register
IRQ152_155_PRIORITY_OFFSET= 0x0498  # IRQ 152-155 priority register
IRQ156_159_PRIORITY_OFFSET= 0x049c  # IRQ 156-159 priority register
IRQ160_163_PRIORITY_OFFSET= 0x04a0  # IRQ 160-163 priority register
IRQ164_167_PRIORITY_OFFSET= 0x04a4  # IRQ 164-167 priority register
IRQ168_171_PRIORITY_OFFSET= 0x04a8  # IRQ 168-171 priority register
IRQ172_175_PRIORITY_OFFSET= 0x04ac  # IRQ 172-175 priority register
IRQ176_179_PRIORITY_OFFSET= 0x04b0  # IRQ 176-179 priority register
IRQ180_183_PRIORITY_OFFSET= 0x04b4  # IRQ 180-183 priority register
IRQ184_187_PRIORITY_OFFSET= 0x04b8  # IRQ 184-187 priority register
IRQ188_191_PRIORITY_OFFSET= 0x04bc  # IRQ 188-191 priority register
IRQ192_195_PRIORITY_OFFSET= 0x04c0  # IRQ 192-195 priority register
IRQ196_199_PRIORITY_OFFSET= 0x04c4  # IRQ 196-199 priority register
IRQ200_203_PRIORITY_OFFSET= 0x04c8  # IRQ 200-203 priority register
IRQ204_207_PRIORITY_OFFSET= 0x04cc  # IRQ 204-207 priority register
IRQ208_211_PRIORITY_OFFSET= 0x04d0  # IRQ 208-211 priority register
IRQ212_215_PRIORITY_OFFSET= 0x04d4  # IRQ 212-215 priority register
IRQ216_219_PRIORITY_OFFSET= 0x04d8  # IRQ 216-219 priority register
IRQ220_223_PRIORITY_OFFSET= 0x04dc  # IRQ 220-223 priority register
IRQ224_227_PRIORITY_OFFSET= 0x04e0  # IRQ 224-227 priority register
IRQ228_231_PRIORITY_OFFSET= 0x04e4  # IRQ 228-231 priority register
IRQ232_235_PRIORITY_OFFSET= 0x04e8  # IRQ 232-235 priority register
IRQ236_239_PRIORITY_OFFSET= 0x04ec  # IRQ 236-239 priority register

# System Control Block= (SCB)

CPUID_BASE_OFFSET         = 0x0d00  # CPUID base register
INTCTRL_OFFSET            = 0x0d04  # Interrupt control state register
VECTAB_OFFSET             = 0x0d08  # Vector table offset register
AIRCR_OFFSET              = 0x0d0c  # Application interrupt/reset control register
SYSCON_OFFSET             = 0x0d10  # System control register
CFGCON_OFFSET             = 0x0d14  # Configuration control register
SYSH4_7_PRIORITY_OFFSET   = 0x0d18  # System handlers 4-7 priority register
SYSH8_11_PRIORITY_OFFSET  = 0x0d1c  # System handler 8-11 priority register
SYSH12_15_PRIORITY_OFFSET = 0x0d20  # System handler 12-15 priority register
SYSHCON_OFFSET            = 0x0d24  # System handler control and state register
CFAULTS_OFFSET            = 0x0d28  # Configurable fault status register
HFAULTS_OFFSET            = 0x0d2c  # Hard fault status register
DFAULTS_OFFSET            = 0x0d30  # Debug fault status register
MEMMANAGE_ADDR_OFFSET     = 0x0d34  # Mem manage address register
BFAULT_ADDR_OFFSET        = 0x0d38  # Bus fault address register
AFAULTS_OFFSET            = 0x0d3c  # Auxiliary fault status register
PFR0_OFFSET               = 0x0d40  # Processor feature register 0
PFR1_OFFSET               = 0x0d44  # Processor feature register 1
DFR0_OFFSET               = 0x0d48  # Debug feature register 0
AFR0_OFFSET               = 0x0d4c  # Auxiliary feature register 0
MMFR0_OFFSET              = 0x0d50  # Memory model feature register 0
MMFR1_OFFSET              = 0x0d54  # Memory model feature register 1
MMFR2_OFFSET              = 0x0d58  # Memory model feature register 2
MMFR3_OFFSET              = 0x0d5c  # Memory model feature register 3
ISAR0_OFFSET              = 0x0d60  # ISA feature register 0
ISAR1_OFFSET              = 0x0d64  # ISA feature register 1
ISAR2_OFFSET              = 0x0d68  # ISA feature register 2
ISAR3_OFFSET              = 0x0d6c  # ISA feature register 3
ISAR4_OFFSET              = 0x0d70  # ISA feature register 4
CLIDR_OFFSET              = 0x0d78  # Cache Level ID register= (Cortex-M7)
CTR_OFFSET                = 0x0d7c  # Cache Type register= (Cortex-M7)
CCSIDR_OFFSET             = 0x0d80  # Cache Size ID Register= (Cortex-M7)
CSSELR_OFFSET             = 0x0d84  # Cache Size Selection Register= (Cortex-M7)
CPACR_OFFSET              = 0x0d88  # Coprocessor Access Control Register
DHCSR_OFFSET              = 0x0df0  # Debug Halting Control and Status Register
DCRSR_OFFSET              = 0x0df4  # Debug Core Register Selector Register
DCRDR_OFFSET              = 0x0df8  # Debug Core Register Data Register
DEMCR_OFFSET              = 0x0dfc  # Debug Exception and Monitor Control Register
STIR_OFFSET               = 0x0f00  # Software trigger interrupt register
FPCCR_OFFSET              = 0x0f34  # Floating-point Context Control Register
FPCAR_OFFSET              = 0x0f38  # Floating-point Context Address Register
FPDSCR_OFFSET             = 0x0f3c  # Floating-point Default Status Control Register
MVFR0_OFFSET              = 0x0f40  # Media and VFP Feature Register 0
MVFR1_OFFSET              = 0x0f44  # Media and VFP Feature Register 1
MVFR2_OFFSET              = 0x0f48  # Media and VFP Feature Register 2
ICIALLU_OFFSET            = 0x0f50  # I-Cache Invalidate All to PoU= (Cortex-M7)
ICIMVAU_OFFSET            = 0x0f58  # I-Cache Invalidate by MVA to PoU= (Cortex-M7)
DCIMVAC_OFFSET            = 0x0f5c  # D-Cache Invalidate by MVA to PoC= (Cortex-M7)
DCISW_OFFSET              = 0x0f60  # D-Cache Invalidate by Set-way= (Cortex-M7)
DCCMVAU_OFFSET            = 0x0f64  # D-Cache Clean by MVA to PoU= (Cortex-M7)
DCCMVAC_OFFSET            = 0x0f68  # D-Cache Clean by MVA to PoC= (Cortex-M7)
DCCSW_OFFSET              = 0x0f6c  # D-Cache Clean by Set-way= (Cortex-M7)
DCCIMVAC_OFFSET           = 0x0f70  # D-Cache Clean and Invalidate by MVA to PoC= (Cortex-M7)
DCCISW_OFFSET             = 0x0f74  # D-Cache Clean and Invalidate by Set-way= (Cortex-M7)
ITCMCR_OFFSET             = 0x0f90  # Instruction Tightly-Coupled Memory Control Register
DTCMCR_OFFSET             = 0x0f94  # Data Tightly-Coupled Memory Control Registers
AHBPCR_OFFSET             = 0x0f98  # AHBP Control Register
CACR_OFFSET               = 0x0f9c  # L1 Cache Control Register
AHBSCR_OFFSET             = 0x0fa0  # AHB Slave Control Register
ABFSR_OFFSET              = 0x0fa8  # Auxiliary Bus Fault Status
PID4_OFFSET               = 0x0fd0  # Peripheral identification register= (PID4)
PID5_OFFSET               = 0x0fd4  # Peripheral identification register= (PID5)
PID6_OFFSET               = 0x0fd8  # Peripheral identification register= (PID6)
PID7_OFFSET               = 0x0fdc  # Peripheral identification register= (PID7)
PID0_OFFSET               = 0x0fe0  # Peripheral identification register bits 7:0= (PID0)
PID1_OFFSET               = 0x0fe4  # Peripheral identification register bits 15:8= (PID1)
PID2_OFFSET               = 0x0fe8  # Peripheral identification register bits 23:16= (PID2)
PID3_OFFSET               = 0x0fec  # Peripheral identification register bits 23:16= (PID3)
CID0_OFFSET               = 0x0ff0  # Component identification register bits 7:0= (CID0)
CID1_OFFSET               = 0x0ff4  # Component identification register bits 15:8= (CID0)
CID2_OFFSET               = 0x0ff8  # Component identification register bits 23:16= (CID0)
CID3_OFFSET               = 0x0ffc  # Component identification register bits 23:16= (CID0)

# NVIC register addresses *****************************************************************

ICTR                      = (ARMV7M_NVIC_BASE + ICTR_OFFSET)
SYSTICK_CTRL              = (ARMV7M_NVIC_BASE + SYSTICK_CTRL_OFFSET)
SYSTICK_RELOAD            = (ARMV7M_NVIC_BASE + SYSTICK_RELOAD_OFFSET)
SYSTICK_CURRENT           = (ARMV7M_NVIC_BASE + SYSTICK_CURRENT_OFFSET)
SYSTICK_CALIB             = (ARMV7M_NVIC_BASE + SYSTICK_CALIB_OFFSET)

IRQ0_31_ENABLE            = (ARMV7M_NVIC_BASE + IRQ0_31_ENABLE_OFFSET)
IRQ32_63_ENABLE           = (ARMV7M_NVIC_BASE + IRQ32_63_ENABLE_OFFSET)
IRQ64_95_ENABLE           = (ARMV7M_NVIC_BASE + IRQ64_95_ENABLE_OFFSET)
IRQ96_127_ENABLE          = (ARMV7M_NVIC_BASE + IRQ96_127_ENABLE_OFFSET)
IRQ128_159_ENABLE         = (ARMV7M_NVIC_BASE + IRQ128_159_ENABLE_OFFSET)
IRQ160_191_ENABLE         = (ARMV7M_NVIC_BASE + IRQ160_191_ENABLE_OFFSET)
IRQ192_223_ENABLE         = (ARMV7M_NVIC_BASE + IRQ192_223_ENABLE_OFFSET)
IRQ224_239_ENABLE         = (ARMV7M_NVIC_BASE + IRQ224_239_ENABLE_OFFSET)

IRQ0_31_CLEAR             = (ARMV7M_NVIC_BASE + IRQ0_31_CLEAR_OFFSET)
IRQ32_63_CLEAR            = (ARMV7M_NVIC_BASE + IRQ32_63_CLEAR_OFFSET)
IRQ64_95_CLEAR            = (ARMV7M_NVIC_BASE + IRQ64_95_CLEAR_OFFSET)
IRQ96_127_CLEAR           = (ARMV7M_NVIC_BASE + IRQ96_127_CLEAR_OFFSET)
IRQ128_159_CLEAR          = (ARMV7M_NVIC_BASE + IRQ128_159_CLEAR_OFFSET)
IRQ160_191_CLEAR          = (ARMV7M_NVIC_BASE + IRQ160_191_CLEAR_OFFSET)
IRQ192_223_CLEAR          = (ARMV7M_NVIC_BASE + IRQ192_223_CLEAR_OFFSET)
IRQ224_239_CLEAR          = (ARMV7M_NVIC_BASE + IRQ224_239_CLEAR_OFFSET)

IRQ0_31_PEND              = (ARMV7M_NVIC_BASE + IRQ0_31_PEND_OFFSET)
IRQ32_63_PEND             = (ARMV7M_NVIC_BASE + IRQ32_63_PEND_OFFSET)
IRQ64_95_PEND             = (ARMV7M_NVIC_BASE + IRQ64_95_PEND_OFFSET)
IRQ96_127_PEND            = (ARMV7M_NVIC_BASE + IRQ96_127_PEND_OFFSET)
IRQ128_159_PEND           = (ARMV7M_NVIC_BASE + IRQ128_159_PEND_OFFSET)
IRQ160_191_PEND           = (ARMV7M_NVIC_BASE + IRQ160_191_PEND_OFFSET)
IRQ192_223_PEND           = (ARMV7M_NVIC_BASE + IRQ192_223_PEND_OFFSET)
IRQ224_239_PEND           = (ARMV7M_NVIC_BASE + IRQ224_239_PEND_OFFSET)

IRQ0_31_CLRPEND           = (ARMV7M_NVIC_BASE + IRQ0_31_CLRPEND_OFFSET)
IRQ32_63_CLRPEND          = (ARMV7M_NVIC_BASE + IRQ32_63_CLRPEND_OFFSET)
IRQ64_95_CLRPEND          = (ARMV7M_NVIC_BASE + IRQ64_95_CLRPEND_OFFSET)
IRQ96_127_CLRPEND         = (ARMV7M_NVIC_BASE + IRQ96_127_CLRPEND_OFFSET)
IRQ128_159_CLRPEND        = (ARMV7M_NVIC_BASE + IRQ128_159_CLRPEND_OFFSET)
IRQ160_191_CLRPEND        = (ARMV7M_NVIC_BASE + IRQ160_191_CLRPEND_OFFSET)
IRQ192_223_CLRPEND        = (ARMV7M_NVIC_BASE + IRQ192_223_CLRPEND_OFFSET)
IRQ224_239_CLRPEND        = (ARMV7M_NVIC_BASE + IRQ224_239_CLRPEND_OFFSET)

IRQ0_31_ACTIVE            = (ARMV7M_NVIC_BASE + IRQ0_31_ACTIVE_OFFSET)
IRQ32_63_ACTIVE           = (ARMV7M_NVIC_BASE + IRQ32_63_ACTIVE_OFFSET)
IRQ64_95_ACTIVE           = (ARMV7M_NVIC_BASE + IRQ64_95_ACTIVE_OFFSET)
IRQ96_127_ACTIVE          = (ARMV7M_NVIC_BASE + IRQ96_127_ACTIVE_OFFSET)
IRQ128_159_ACTIVE         = (ARMV7M_NVIC_BASE + IRQ128_159_ACTIVE_OFFSET)
IRQ160_191_ACTIVE         = (ARMV7M_NVIC_BASE + IRQ160_191_ACTIVE_OFFSET)
IRQ192_223_ACTIVE         = (ARMV7M_NVIC_BASE + IRQ192_223_ACTIVE_OFFSET)
IRQ224_239_ACTIVE         = (ARMV7M_NVIC_BASE + IRQ224_239_ACTIVE_OFFSET)

IRQ0_3_PRIORITY           = (ARMV7M_NVIC_BASE + IRQ0_3_PRIORITY_OFFSET)
IRQ4_7_PRIORITY           = (ARMV7M_NVIC_BASE + IRQ4_7_PRIORITY_OFFSET)
IRQ8_11_PRIORITY          = (ARMV7M_NVIC_BASE + IRQ8_11_PRIORITY_OFFSET)
IRQ12_15_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ12_15_PRIORITY_OFFSET)
IRQ16_19_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ16_19_PRIORITY_OFFSET)
IRQ20_23_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ20_23_PRIORITY_OFFSET)
IRQ24_27_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ24_27_PRIORITY_OFFSET)
IRQ28_31_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ28_31_PRIORITY_OFFSET)
IRQ32_35_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ32_35_PRIORITY_OFFSET)
IRQ36_39_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ36_39_PRIORITY_OFFSET)
IRQ40_43_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ40_43_PRIORITY_OFFSET)
IRQ44_47_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ44_47_PRIORITY_OFFSET)
IRQ48_51_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ48_51_PRIORITY_OFFSET)
IRQ52_55_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ52_55_PRIORITY_OFFSET)
IRQ56_59_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ56_59_PRIORITY_OFFSET)
IRQ60_63_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ60_63_PRIORITY_OFFSET)
IRQ64_67_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ64_67_PRIORITY_OFFSET)
IRQ68_71_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ68_71_PRIORITY_OFFSET)
IRQ72_75_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ72_75_PRIORITY_OFFSET)
IRQ76_79_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ76_79_PRIORITY_OFFSET)
IRQ80_83_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ80_83_PRIORITY_OFFSET)
IRQ84_87_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ84_87_PRIORITY_OFFSET)
IRQ88_91_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ88_91_PRIORITY_OFFSET)
IRQ92_95_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ92_95_PRIORITY_OFFSET)
IRQ96_99_PRIORITY         = (ARMV7M_NVIC_BASE + IRQ96_99_PRIORITY_OFFSET)
IRQ100_103_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ100_103_PRIORITY_OFFSET)
IRQ104_107_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ104_107_PRIORITY_OFFSET)
IRQ108_111_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ108_111_PRIORITY_OFFSET)
IRQ112_115_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ112_115_PRIORITY_OFFSET)
IRQ116_119_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ116_119_PRIORITY_OFFSET)
IRQ120_123_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ120_123_PRIORITY_OFFSET)
IRQ124_127_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ124_127_PRIORITY_OFFSET)
IRQ128_131_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ128_131_PRIORITY_OFFSET)
IRQ132_135_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ132_135_PRIORITY_OFFSET)
IRQ136_139_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ136_139_PRIORITY_OFFSET)
IRQ140_143_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ140_143_PRIORITY_OFFSET)
IRQ144_147_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ144_147_PRIORITY_OFFSET)
IRQ148_151_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ148_151_PRIORITY_OFFSET)
IRQ152_155_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ152_155_PRIORITY_OFFSET)
IRQ156_159_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ156_159_PRIORITY_OFFSET)
IRQ160_163_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ160_163_PRIORITY_OFFSET)
IRQ164_167_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ164_167_PRIORITY_OFFSET)
IRQ168_171_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ168_171_PRIORITY_OFFSET)
IRQ172_175_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ172_175_PRIORITY_OFFSET)
IRQ176_179_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ176_179_PRIORITY_OFFSET)
IRQ180_183_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ180_183_PRIORITY_OFFSET)
IRQ184_187_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ184_187_PRIORITY_OFFSET)
IRQ188_191_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ188_191_PRIORITY_OFFSET)
IRQ192_195_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ192_195_PRIORITY_OFFSET)
IRQ196_199_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ196_199_PRIORITY_OFFSET)
IRQ200_203_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ200_203_PRIORITY_OFFSET)
IRQ204_207_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ204_207_PRIORITY_OFFSET)
IRQ208_211_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ208_211_PRIORITY_OFFSET)
IRQ212_215_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ212_215_PRIORITY_OFFSET)
IRQ216_219_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ216_219_PRIORITY_OFFSET)
IRQ220_223_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ220_223_PRIORITY_OFFSET)
IRQ224_227_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ224_227_PRIORITY_OFFSET)
IRQ228_231_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ228_231_PRIORITY_OFFSET)
IRQ232_235_PRIORITY       = (ARMV7M_NVIC_BASE + IRQ232_235_PRIORITY_OFFSET)

CPUID_BASE                = (ARMV7M_NVIC_BASE + CPUID_BASE_OFFSET)
INTCTRL                   = (ARMV7M_NVIC_BASE + INTCTRL_OFFSET)
VECTAB                    = (ARMV7M_NVIC_BASE + VECTAB_OFFSET)
AIRCR                     = (ARMV7M_NVIC_BASE + AIRCR_OFFSET)
SYSCON                    = (ARMV7M_NVIC_BASE + SYSCON_OFFSET)
CFGCON                    = (ARMV7M_NVIC_BASE + CFGCON_OFFSET)
SYSH4_7_PRIORITY          = (ARMV7M_NVIC_BASE + SYSH4_7_PRIORITY_OFFSET)
SYSH8_11_PRIORITY         = (ARMV7M_NVIC_BASE + SYSH8_11_PRIORITY_OFFSET)
SYSH12_15_PRIORITY        = (ARMV7M_NVIC_BASE + SYSH12_15_PRIORITY_OFFSET)
SYSHCON                   = (ARMV7M_NVIC_BASE + SYSHCON_OFFSET)
CFAULTS                   = (ARMV7M_NVIC_BASE + CFAULTS_OFFSET)
HFAULTS                   = (ARMV7M_NVIC_BASE + HFAULTS_OFFSET)
DFAULTS                   = (ARMV7M_NVIC_BASE + DFAULTS_OFFSET)
MEMMANAGE_ADDR            = (ARMV7M_NVIC_BASE + MEMMANAGE_ADDR_OFFSET)
BFAULT_ADDR               = (ARMV7M_NVIC_BASE + BFAULT_ADDR_OFFSET)
AFAULTS                   = (ARMV7M_NVIC_BASE + AFAULTS_OFFSET)
PFR0                      = (ARMV7M_NVIC_BASE + PFR0_OFFSET)
PFR1                      = (ARMV7M_NVIC_BASE + PFR1_OFFSET)
DFR0                      = (ARMV7M_NVIC_BASE + DFR0_OFFSET)
AFR0                      = (ARMV7M_NVIC_BASE + AFR0_OFFSET)
MMFR0                     = (ARMV7M_NVIC_BASE + MMFR0_OFFSET)
MMFR1                     = (ARMV7M_NVIC_BASE + MMFR1_OFFSET)
MMFR2                     = (ARMV7M_NVIC_BASE + MMFR2_OFFSET)
MMFR3                     = (ARMV7M_NVIC_BASE + MMFR3_OFFSET)
ISAR0                     = (ARMV7M_NVIC_BASE + ISAR0_OFFSET)
ISAR1                     = (ARMV7M_NVIC_BASE + ISAR1_OFFSET)
ISAR2                     = (ARMV7M_NVIC_BASE + ISAR2_OFFSET)
ISAR3                     = (ARMV7M_NVIC_BASE + ISAR3_OFFSET)
ISAR4                     = (ARMV7M_NVIC_BASE + ISAR4_OFFSET)
CLIDR                     = (ARMV7M_NVIC_BASE + CLIDR_OFFSET)
CTR                       = (ARMV7M_NVIC_BASE + CTR_OFFSET)
CCSIDR                    = (ARMV7M_NVIC_BASE + CCSIDR_OFFSET)
CSSELR                    = (ARMV7M_NVIC_BASE + CSSELR_OFFSET)
CPACR                     = (ARMV7M_NVIC_BASE + CPACR_OFFSET)
DHCSR                     = (ARMV7M_NVIC_BASE + DHCSR_OFFSET)
DCRSR                     = (ARMV7M_NVIC_BASE + DCRSR_OFFSET)
DCRDR                     = (ARMV7M_NVIC_BASE + DCRDR_OFFSET)
DEMCR                     = (ARMV7M_NVIC_BASE + DEMCR_OFFSET)
STIR                      = (ARMV7M_NVIC_BASE + STIR_OFFSET)
FPCCR                     = (ARMV7M_NVIC_BASE + FPCCR_OFFSET)
ICIALLU                   = (ARMV7M_NVIC_BASE + ICIALLU_OFFSET)
ICIMVAU                   = (ARMV7M_NVIC_BASE + ICIMVAU_OFFSET)
#DCIMVAU                   = (ARMV7M_NVIC_BASE + DCIMVAU_OFFSET)
DCIMVAC                   = (ARMV7M_NVIC_BASE + DCIMVAC_OFFSET)
DCISW                     = (ARMV7M_NVIC_BASE + DCISW_OFFSET)
DCCMVAU                   = (ARMV7M_NVIC_BASE + DCCMVAU_OFFSET)
DCCMVAC                   = (ARMV7M_NVIC_BASE + DCCMVAC_OFFSET)
DCCSW                     = (ARMV7M_NVIC_BASE + DCCSW_OFFSET)
DCCIMVAC                  = (ARMV7M_NVIC_BASE + DCCIMVAC_OFFSET)
DCCISW                    = (ARMV7M_NVIC_BASE + DCCISW_OFFSET)
ITCMCR                    = (ARMV7M_NVIC_BASE + ITCMCR_OFFSET)
DTCMCR                    = (ARMV7M_NVIC_BASE + DTCMCR_OFFSET)
AHBPCR                    = (ARMV7M_NVIC_BASE + AHBPCR_OFFSET)
CACR                      = (ARMV7M_NVIC_BASE + CACR_OFFSET)
AHBSCR                    = (ARMV7M_NVIC_BASE + AHBSCR_OFFSET)
ABFSR                     = (ARMV7M_NVIC_BASE + ABFSR_OFFSET)
PID4                      = (ARMV7M_NVIC_BASE + PID4_OFFSET)
PID5                      = (ARMV7M_NVIC_BASE + PID5_OFFSET)
PID6                      = (ARMV7M_NVIC_BASE + PID6_OFFSET)
PID7                      = (ARMV7M_NVIC_BASE + PID7_OFFSET)
PID0                      = (ARMV7M_NVIC_BASE + PID0_OFFSET)
PID1                      = (ARMV7M_NVIC_BASE + PID1_OFFSET)
PID2                      = (ARMV7M_NVIC_BASE + PID2_OFFSET)
PID3                      = (ARMV7M_NVIC_BASE + PID3_OFFSET)
CID0                      = (ARMV7M_NVIC_BASE + CID0_OFFSET)
CID1                      = (ARMV7M_NVIC_BASE + CID1_OFFSET)
CID2                      = (ARMV7M_NVIC_BASE + CID2_OFFSET)
CID3                      = (ARMV7M_NVIC_BASE + CID3_OFFSET)

# NVIC register bit definitions ***********************************************************

# Interrupt controller type= (INCTCTL_TYPE)

ICTR_INTLINESNUM_SHIFT    = 0    # Bits 0-3: Number of interrupt inputs / 32 - 1
ICTR_INTLINESNUM_MASK     = (15 << ICTR_INTLINESNUM_SHIFT)

# SysTick control and status register= (SYSTICK_CTRL)

SYSTICK_CTRL_ENABLE       = (1 << 0)  # Bit 0:  Enable
SYSTICK_CTRL_TICKINT      = (1 << 1)  # Bit 1:  Tick interrupt
SYSTICK_CTRL_CLKSOURCE    = (1 << 2)  # Bit 2:  Clock source
SYSTICK_CTRL_COUNTFLAG    = (1 << 16) # Bit 16: Count Flag

# SysTick reload value register= (SYSTICK_RELOAD)

SYSTICK_RELOAD_SHIFT      = 0         # Bits 23-0: Timer reload value
SYSTICK_RELOAD_MASK       = (0x00ffffff << SYSTICK_RELOAD_SHIFT)

# SysTick current value register= (SYSTICK_CURRENT)

SYSTICK_CURRENT_SHIFT     = 0         # Bits 23-0: Timer current value
SYSTICK_CURRENT_MASK      = (0x00ffffff << SYSTICK_RELOAD_SHIFT)

# SysTick calibration value register= (SYSTICK_CALIB)

SYSTICK_CALIB_TENMS_SHIFT = 0         # Bits 23-0: Calibration value
SYSTICK_CALIB_TENMS_MASK  = (0x00ffffff << SYSTICK_CALIB_TENMS_SHIFT)
SYSTICK_CALIB_SKEW        = (1 << 30) # Bit 30: Calibration value inexact
SYSTICK_CALIB_NOREF       = (1 << 31) # Bit 31: No external reference clock

# Interrupt control state register= (INTCTRL)

INTCTRL_NMIPENDSET        = (1 << 31) # Bit 31: Set pending NMI bit
INTCTRL_PENDSVSET         = (1 << 28) # Bit 28: Set pending PendSV bit
INTCTRL_PENDSVCLR         = (1 << 27) # Bit 27: Clear pending PendSV bit
INTCTRL_PENDSTSET         = (1 << 26) # Bit 26: Set pending SysTick bit
INTCTRL_PENDSTCLR         = (1 << 25) # Bit 25: Clear pending SysTick bit
INTCTRL_ISPREEMPOT        = (1 << 23) # Bit 23: Pending active next cycle
INTCTRL_ISRPENDING        = (1 << 22) # Bit 22: Interrupt pending flag
INTCTRL_VECTPENDING_SHIFT = 12        # Bits 21-12: Pending ISR number field
INTCTRL_VECTPENDING_MASK  = (0x3ff << INTCTRL_VECTPENDING_SHIFT)
INTCTRL_RETTOBASE         = (1 << 11) # Bit 11: no other exceptions pending
INTCTRL_VECTACTIVE_SHIFT  = 0         # Bits 8-0: Active ISR number
INTCTRL_VECTACTIVE_MASK   = (0x1ff << INTCTRL_VECTACTIVE_SHIFT)

# System control register= (SYSCON)
                                                  # Bit 0:  Reserved
SYSCON_SLEEPONEXIT        = (1 << 1)  # Bit 1:  Sleep-on-exit= (returning from Handler to Thread mode)
SYSCON_SLEEPDEEP          = (1 << 2)  # Bit 2: Use deep sleep in low power mode
                                                  # Bit 3:  Reserved
SYSCON_SEVONPEND          = (1 << 4)  # Bit 4: Send Event on Pending bit
                                                  # Bits 5-31: Reserved

# Configuration control register= (CFGCON)

CFGCON_NONBASETHRDENA     = (1 << 0)  # Bit 0: How processor enters thread mode
CFGCON_USERSETMPEND       = (1 << 1)  # Bit 1: Enables unprivileged access to STIR
CFGCON_UNALIGNTRP         = (1 << 3)  # Bit 3: Enables unaligned access traps
CFGCON_DIV0TRP            = (1 << 4)  # Bit 4: Enables fault on divide-by-zero
CFGCON_BFHFNMIGN          = (1 << 8)  # Bit 8: Disables data bus faults
CFGCON_STKALIGN           = (1 << 9)  # Bit 9: Indicates stack alignment on exeption
                                                  # Cortex-M7:
CFGCON_DC                 = (1 << 16) # Bit 16: Data cache enable
CFGCON_IC                 = (1 << 17) # Bit 17: Instruction cache enable
CFGCON_BP                 = (1 << 18) # Bit 18: Branch prediction enable

# System handler 4-7 priority register

SYSH_PRIORITY_PR4_SHIFT   = 0
SYSH_PRIORITY_PR4_MASK    = (0xff << SYSH_PRIORITY_PR4_SHIFT)
SYSH_PRIORITY_PR5_SHIFT   = 8
SYSH_PRIORITY_PR5_MASK    = (0xff << SYSH_PRIORITY_PR5_SHIFT)
SYSH_PRIORITY_PR6_SHIFT   = 16
SYSH_PRIORITY_PR6_MASK    = (0xff << SYSH_PRIORITY_PR6_SHIFT)
SYSH_PRIORITY_PR7_SHIFT   = 24
SYSH_PRIORITY_PR7_MASK    = (0xff << SYSH_PRIORITY_PR7_SHIFT)

# System handler 8-11 priority register

SYSH_PRIORITY_PR8_SHIFT   = 0
SYSH_PRIORITY_PR8_MASK    = (0xff << SYSH_PRIORITY_PR8_SHIFT)
SYSH_PRIORITY_PR9_SHIFT   = 8
SYSH_PRIORITY_PR9_MASK    = (0xff << SYSH_PRIORITY_PR9_SHIFT)
SYSH_PRIORITY_PR10_SHIFT  = 16
SYSH_PRIORITY_PR10_MASK   = (0xff << SYSH_PRIORITY_PR10_SHIFT)
SYSH_PRIORITY_PR11_SHIFT  = 24
SYSH_PRIORITY_PR11_MASK   = (0xff << SYSH_PRIORITY_PR11_SHIFT)

# System handler 12-15 priority register

SYSH_PRIORITY_PR12_SHIFT  = 0
SYSH_PRIORITY_PR12_MASK   = (0xff << SYSH_PRIORITY_PR12_SHIFT)
SYSH_PRIORITY_PR13_SHIFT  = 8
SYSH_PRIORITY_PR13_MASK   = (0xff << SYSH_PRIORITY_PR13_SHIFT)
SYSH_PRIORITY_PR14_SHIFT  = 16
SYSH_PRIORITY_PR14_MASK   = (0xff << SYSH_PRIORITY_PR14_SHIFT)
SYSH_PRIORITY_PR15_SHIFT  = 24
SYSH_PRIORITY_PR15_MASK   = (0xff << SYSH_PRIORITY_PR15_SHIFT)

# Application Interrupt and Reset Control Register= (AIRCR)

AIRCR_VECTRESET           = (1 << 0)  # Bit 0:  VECTRESET
AIRCR_VECTCLRACTIVE       = (1 << 1)  # Bit 1:  Reserved for debug use
AIRCR_SYSRESETREQ         = (1 << 2)  # Bit 2:  System reset
                                                  # Bits 2-7:  Reserved
AIRCR_PRIGROUP_SHIFT      = (8)       # Bits 8-14: PRIGROUP
AIRCR_PRIGROUP_MASK       = (7 << AIRCR_PRIGROUP_SHIFT)
AIRCR_ENDIANNESS          = (1 << 15) # Bit 15: 1=Big endian
AIRCR_VECTKEY_SHIFT       = (16)      # Bits 16-31: VECTKEY
AIRCR_VECTKEY_MASK        = (0xffff << AIRCR_VECTKEY_SHIFT)
AIRCR_VECTKEYSTAT_SHIFT   = (16)      # Bits 16-31: VECTKEYSTAT
AIRCR_VECTKEYSTAT_MASK    = (0xffff << AIRCR_VECTKEYSTAT_SHIFT)

# System handler control and state register= (SYSHCON)

SYSHCON_MEMFAULTACT       = (1 << 0)  # Bit 0:  MemManage is active
SYSHCON_BUSFAULTACT       = (1 << 1)  # Bit 1:  BusFault is active
SYSHCON_USGFAULTACT       = (1 << 3)  # Bit 3:  UsageFault is active
SYSHCON_SVCALLACT         = (1 << 7)  # Bit 7:  SVCall is active
SYSHCON_MONITORACT        = (1 << 8)  # Bit 8:  Monitor is active
SYSHCON_PENDSVACT         = (1 << 10) # Bit 10: PendSV is active
SYSHCON_SYSTICKACT        = (1 << 11) # Bit 11: SysTick is active
SYSHCON_USGFAULTPENDED    = (1 << 12) # Bit 12: Usage fault is pended
SYSHCON_MEMFAULTPENDED    = (1 << 13) # Bit 13: MemManage is pended
SYSHCON_BUSFAULTPENDED    = (1 << 14) # Bit 14: BusFault is pended
SYSHCON_SVCALLPENDED      = (1 << 15) # Bit 15: SVCall is pended
SYSHCON_MEMFAULTENA       = (1 << 16) # Bit 16: MemFault enabled
SYSHCON_BUSFAULTENA       = (1 << 17) # Bit 17: BusFault enabled
SYSHCON_USGFAULTENA       = (1 << 18) # Bit 18: UsageFault enabled

# Cache Level ID register= (Cortex-M7)

CLIDR_L1CT_SHIFT          = (0)      # Bits 0-2: Level 1 cache type
CLIDR_L1CT_MASK           = (7 << CLIDR_L1CT_SHIFT)
CLIDR_LOC_SHIFT           = (24)      # Bits 24-26: Level of Coherency
CLIDR_L1CT_ICACHE         = (1 << CLIDR_LOC_SHIFT)
CLIDR_L1CT_DCACHE         = (2 << CLIDR_LOC_SHIFT)
CLIDR_LOC_MASK            = (7 << CLIDR_LOC_SHIFT)
CLIDR_LOC_IMPLEMENTED     = (1 << CLIDR_LOC_SHIFT)
CLIDR_LOC_UNIMPLEMENTED   = (0 << CLIDR_LOC_SHIFT)
CLIDR_LOUU_SHIFT          = (27)      # Bits 27-29: Level of Unification Uniprocessor
CLIDR_LOUU_MASK           = (7 << CLIDR_LOUU_SHIFT)
CLIDR_LOUU_IMPLEMENTED    = (1 << CLIDR_LOUU_SHIFT)
CLIDR_LOUU_UNIMPLEMENTED  = (0 << CLIDR_LOUU_SHIFT)

# Cache Type register= (Cortex-M7)

CTR_IMINLINE_SHIFT        = (0)       # Bits 0-3: ImInLine
CTR_IMINLINE_MASK         = (15 << CTR_IMINLINE_SHIFT)
CTR_DMINLINE_SHIFT        = (16)      # Bits 16-19: DmInLine
CTR_DMINLINE_MASK         = (15 << CTR_DMINLINE_SHIFT)
CTR_ERG_SHIFT             = (20)      # Bits 20-23: ERG
CTR_ERG_MASK              = (15 << CTR_ERG_SHIFT)
CTR_CWG_SHIFT             = (24)      # Bits 24-27: ERG
CTR_CWG_MASK              = (15 << CTR_CWG_SHIFT)
CTR_FORMAT_SHIFT          = (29)      # Bits 29-31: Format
CTR_FORMAT_MASK           = (7 << CTR_FORMAT_SHIFT)

# Cache Size ID Register= (Cortex-M7)

CCSIDR_LINESIZE_SHIFT     = (0)       # Bits 0-2: Number of words in each cache line
CCSIDR_LINESIZE_MASK      = (7 << CCSIDR_LINESIZE_SHIFT)
CCSIDR_ASSOCIATIVITY_SHIFT= (3)       # Bits 3-12: Number of ways - 1
CCSIDR_ASSOCIATIVITY_MASK = (0x3ff << CCSIDR_ASSOCIATIVITY_SHIFT)
CCSIDR_NUMSETS_SHIFT      = (13)      # Bits 13-27: Number of sets - 1
CCSIDR_NUMSETS_MASK       = (0x7fff << CCSIDR_NUMSETS_SHIFT)
CCSIDR_WA_SHIFT           = (1 << 28) # Bits 28: Write Allocation support
CCSIDR_RA_SHIFT           = (1 << 29) # Bits 29: Read Allocation support
CCSIDR_WB_SHIFT           = (1 << 30) # Bits 30: Write-Back support
CCSIDR_WT_SHIFT           = (1 << 31) # Bits 31: Write-Through support

# Cache Size Selection Register= (Cortex-M7)

CSSELR_IND                = (1 << 0)  # Bit 0: Selects either instruction or data cache
CSSELR_IND_ICACHE         = (0 << 0)  #   0=Instruction Cache
CSSELR_IND_DCACHE         = (1 << 0)  #   1=Data Cache

CSSELR_LEVEL_SHIFT        = (1)       # Bit 1-3: Selects cache level
CSSELR_LEVEL_MASK         = (7 << CSSELR_LEVEL_SHIFT)
CSSELR_LEVEL_1            = (0 << CSSELR_LEVEL_SHIFT)

# Debug Exception and Monitor Control Register= (DEMCR)

DEMCR_VCCORERESET         = (1 << 0)  # Bit 0:  Reset Vector Catch
DEMCR_VCMMERR             = (1 << 4)  # Bit 4:  Debug trap on Memory Management faults
DEMCR_VCNOCPERR           = (1 << 5)  # Bit 5:  Debug trap on Usage Fault access to non-present coprocessor
DEMCR_VCCHKERR            = (1 << 6)  # Bit 6:  Debug trap on Usage Fault enabled checking errors
DEMCR_VCSTATERR           = (1 << 7)  # Bit 7:  Debug trap on Usage Fault state error
DEMCR_VCBUSERR            = (1 << 8)  # Bit 8:  Debug Trap on normal Bus error
DEMCR_VCINTERR            = (1 << 9)  # Bit 9:  Debug Trap on interrupt/exception service errors
DEMCR_VCHARDERR           = (1 << 10) # Bit 10: Debug trap on Hard Fault
DEMCR_MONEN               = (1 << 16) # Bit 16: Enable the debug monitor
DEMCR_MONPEND             = (1 << 17) # Bit 17: Pend the monitor to activate when priority permits
DEMCR_MONSTEP             = (1 << 18) # Bit 18: Steps the core
DEMCR_MONREQ              = (1 << 19) # Bit 19: Monitor wake-up mode
DEMCR_TRCENA              = (1 << 24) # Bit 24: Enable trace and debug blocks

# Instruction Tightly-Coupled Memory Control Register= (ITCMCR)
# Data Tightly-Coupled Memory Control Registers= (DTCMCR

TCMCR_EN                  = (1 << 0)  # Bit 9:  TCM enable
TCMCR_RMW                 = (1 << 1)  # Bit 1:  Read-Modify-Write= (RMW) enable
TCMCR_RETEN               = (1 << 2)  # Bit 2:  Retry phase enable
TCMCR_SZ_SHIFT            = (3)       # Bits 3-6: Size of the TCM
TCMCR_SZ_MASK             = (15 << TCMCR_SZ_SHIFT)
TCMCR_SZ_NONE             = (0 << TCMCR_SZ_SHIFT) # No TCM implemented
TCMCR_SZ_4KB              = (3 << TCMCR_SZ_SHIFT)
TCMCR_SZ_8KB              = (4 << TCMCR_SZ_SHIFT)
TCMCR_SZ_16KB             = (5 << TCMCR_SZ_SHIFT)
TCMCR_SZ_32KB             = (6 << TCMCR_SZ_SHIFT)
TCMCR_SZ_64KB             = (7 << TCMCR_SZ_SHIFT)
TCMCR_SZ_128KB            = (8 << TCMCR_SZ_SHIFT)
TCMCR_SZ_256KB            = (9 << TCMCR_SZ_SHIFT)
TCMCR_SZ_512KB            = (10 << TCMCR_SZ_SHIFT)
TCMCR_SZ_1MB              = (11 << TCMCR_SZ_SHIFT)
TCMCR_SZ_2MB              = (12 << TCMCR_SZ_SHIFT)
TCMCR_SZ_4MB              = (13 << TCMCR_SZ_SHIFT)
TCMCR_SZ_8MB              = (14 << TCMCR_SZ_SHIFT)
TCMCR_SZ_16MB             = (15 << TCMCR_SZ_SHIFT)

# AHBP Control Register= (AHBPCR, Cortex-M7)

AHBPCR_EN                 = (1 << 0)  # Bit 0: AHBP enable
AHBPCR_SZ_SHIFT           = (1)       # Bits 1-3: AHBP size
AHBPCR_SZ_MASK            = (7 << AHBPCR_SZ_SHIFT)
AHBPCR_SZ_DISABLED        = (0 << AHBPCR_SZ_SHIFT)
AHBPCR_SZ_64MB            = (1 << AHBPCR_SZ_SHIFT)
AHBPCR_SZ_128MB           = (2 << AHBPCR_SZ_SHIFT)
AHBPCR_SZ_256MB           = (3 << AHBPCR_SZ_SHIFT)
AHBPCR_SZ_512MB           = (4 << AHBPCR_SZ_SHIFT)

# L1 Cache Control Register= (CACR, Cortex-M7)

CACR_SIWT                 = (1 << 0)  # Bit 0:  Shared cacheable-is-WT for data cache
CACR_ECCDIS               = (1 << 1)  # Bit 1:  Enables ECC in the instruction and data cache
CACR_FORCEWT              = (1 << 2)  # Bit 2:  Enables Force Write-Through in the data cache

def IRQ_ENABLE_OFFSET(n):
    return (0x0100 + 4*((n) >> 5))

def IRQ_CLEAR_OFFSET(n):
    return (0x0180 + 4*((n) >> 5))

def IRQ_PEND_OFFSET(n):
    return (0x0200 + 4*((n) >> 5))

def IRQ_CLRPEND_OFFSET(n):
    return (0x0280 + 4*((n) >> 5))

def IRQ_ACTIVE_OFFSET(n):
    return (0x0300 + 4*((n) >> 5))

def IRQ_PRIORITY_OFFSET(n):
    return (0x0400 + 4*((n) >> 2))

def SYSH_PRIORITY_OFFSET(n):
    return (0x0d14 + 4*((n) >> 2))

def IRQ_ENABLE(n):
    return (ARMV7M_NVIC_BASE + IRQ_ENABLE_OFFSET(n))

def IRQ_CLEAR(n):
    return (ARMV7M_NVIC_BASE + IRQ_CLEAR_OFFSET(n))

def IRQ_PEND(n):
    return (ARMV7M_NVIC_BASE + IRQ_PEND_OFFSET(n))

def IRQ_CLRPEND(n):
    return (ARMV7M_NVIC_BASE + IRQ_CLRPEND_OFFSET(n))

def IRQ_ACTIVE(n):
    return (ARMV7M_NVIC_BASE + IRQ_ACTIVE_OFFSET(n))

def IRQ_PRIORITY(n):
    return (ARMV7M_NVIC_BASE + IRQ_PRIORITY_OFFSET(n))

def SYSH_PRIORITY(n):
    return (ARMV7M_NVIC_BASE + SYSH_PRIORITY_OFFSET(n))
