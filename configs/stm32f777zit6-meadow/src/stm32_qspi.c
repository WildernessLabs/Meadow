#include <stdint.h>

#include "stm32_gpio.h"
#include "stm32f777zit6-meadow.h"

#include "stm32_qspi.h"

#define QUADSPI_BASE		0xA0000000
#define QUADSPI_CR_ADDR		(uint32_t*)(QUADSPI_BASE+0x00)
#define QUADSPI_DCR_ADDR	(uint32_t*)(QUADSPI_BASE+0x04)
#define QUADSPI_SR_ADDR		(uint32_t*)(QUADSPI_BASE+0x08)
#define QUADSPI_FCR_ADDR	(uint32_t*)(QUADSPI_BASE+0x0c)
#define QUADSPI_DLR_ADDR	(uint32_t*)(QUADSPI_BASE+0x10)
#define QUADSPI_CCR_ADDR	(uint32_t*)(QUADSPI_BASE+0x14)
#define QUADSPI_AR_ADDR		(uint32_t*)(QUADSPI_BASE+0x18)
#define QUADSPI_DR_ADDR		(uint32_t*)(QUADSPI_BASE+0x20)
#define QUADSPI_PSMKR_ADDR	(uint32_t*)(QUADSPI_BASE+0x24)
#define QUADSPI_PSMAR_ADDR	(uint32_t*)(QUADSPI_BASE+0x28)
#define QUADSPI_PIR_ADDR	(uint32_t*)(QUADSPI_BASE+0x2c)
#define QSPI_NGPIOS 6

static const uint32_t g_gpios[QSPI_NGPIOS] =
{
  GPIO_QUADSPI_BK1_IO0, GPIO_QUADSPI_BK1_IO1, GPIO_QUADSPI_BK1_IO2, GPIO_QUADSPI_BK1_IO3, GPIO_QUADSPI_BK1_NCS, GPIO_QUADSPI_CLK
};

static void stm32_extmemgpios(const uint32_t *gpios, int ngpios)
{
  int i;

  /* Configure GPIOs */

  for (i = 0; i < ngpios; i++)
    {
      stm32_configgpio(gpios[i]);
    }
}

void stm32_quadspi_busy_wait(void)
{
    volatile uint32_t *QUADSPI_SR        = QUADSPI_SR_ADDR;

    while (*QUADSPI_SR & QUADSPI_SR_BUSY);
}

void stm32_quadspi_wait_flag(uint32_t flag)
{
    volatile uint32_t *QUADSPI_SR        = QUADSPI_SR_ADDR;
    volatile uint32_t *QUADSPI_FCR        = QUADSPI_FCR_ADDR;

    while (!(*QUADSPI_SR & flag));
    *QUADSPI_FCR = flag;
}

void stm32_quadspi_write_enable(void)
{
    volatile uint32_t *QUADSPI_CR        = QUADSPI_CR_ADDR;
    volatile uint32_t *QUADSPI_DLR        = QUADSPI_DLR_ADDR;
    volatile uint32_t *QUADSPI_CCR        = QUADSPI_CCR_ADDR;
    volatile uint32_t *QUADSPI_PSMKR    = QUADSPI_PSMKR_ADDR;
    volatile uint32_t *QUADSPI_PSMAR    = QUADSPI_PSMAR_ADDR;
    volatile uint32_t *QUADSPI_PIR        = QUADSPI_PIR_ADDR;

    stm32_quadspi_busy_wait();

    *QUADSPI_CCR = QUADSPI_CCR_FMODE_IND_WR | QUADSPI_CCR_IDMOD_1_LINE |
        WRITE_ENABLE_CMD;

    stm32_quadspi_wait_flag(QUADSPI_SR_TCF);

    stm32_quadspi_busy_wait();

    *QUADSPI_PSMAR = S25FL512S_SR_WREN;
    *QUADSPI_PSMKR = S25FL512S_SR_WREN;
    *QUADSPI_PIR = 0x10;

    *QUADSPI_CR |= QUADSPI_CR_AMPS;
    *QUADSPI_DLR = 0;
    *QUADSPI_CCR = QUADSPI_CCR_FMODE_AUTO_POLL | QUADSPI_CCR_DMODE_1_LINE |
        QUADSPI_CCR_IDMOD_1_LINE | READ_STATUS_REG_CMD;

    stm32_quadspi_wait_flag(QUADSPI_SR_SMF);
}

void stm32_quadspi_init()
{
    volatile uint32_t *QUADSPI_CR        = QUADSPI_CR_ADDR;
    volatile uint32_t *QUADSPI_DCR        = QUADSPI_DCR_ADDR;
    volatile uint32_t *QUADSPI_SR        = QUADSPI_SR_ADDR;
    volatile uint32_t *QUADSPI_DLR        = QUADSPI_DLR_ADDR;
    volatile uint32_t *QUADSPI_CCR        = QUADSPI_CCR_ADDR;
    volatile uint32_t *QUADSPI_AR        = QUADSPI_AR_ADDR;
    volatile uint32_t *QUADSPI_DR        = QUADSPI_DR_ADDR;
    volatile uint32_t *QUADSPI_PSMKR    = QUADSPI_PSMKR_ADDR;
    volatile uint32_t *QUADSPI_PSMAR    = QUADSPI_PSMAR_ADDR;
    volatile uint32_t *QUADSPI_PIR        = QUADSPI_PIR_ADDR;
    uint32_t reg;

    stm32_extmemgpios(g_gpios, QSPI_NGPIOS);

    *QUADSPI_CR = QUADSPI_CR_FTHRES(3); //params->fifo_threshold;

    stm32_quadspi_busy_wait();

    *QUADSPI_CR |= QUADSPI_CR_PRESCALER(2 /*params->prescaler*/) | 0 /*params->sshift*/ |
        0 /*params->dfm*/ | 0 /*params->fsel*/;
    *QUADSPI_DCR = (27 << 16) /*params->fsize*/ | QUADSPI_DCR_CSHT(1);

    *QUADSPI_CR |= QUADSPI_CR_EN;

    /* Reset memory */
    stm32_quadspi_busy_wait();

    *QUADSPI_CCR = QUADSPI_CCR_FMODE_IND_WR | QUADSPI_CCR_IDMOD_1_LINE |
        RESET_ENABLE_CMD;

    stm32_quadspi_wait_flag(QUADSPI_SR_TCF);

    *QUADSPI_CCR = QUADSPI_CCR_FMODE_IND_WR | QUADSPI_CCR_IDMOD_1_LINE |
        RESET_MEMORY_CMD;

    stm32_quadspi_wait_flag(QUADSPI_SR_TCF);

    stm32_quadspi_busy_wait();

    *QUADSPI_PSMAR = 0;
    *QUADSPI_PSMKR = S25FL512S_SR_WIP;
    *QUADSPI_PIR = 0x10;

    *QUADSPI_CR |= QUADSPI_CR_AMPS;
    *QUADSPI_DLR = 0;
    *QUADSPI_CCR = QUADSPI_CCR_FMODE_AUTO_POLL | QUADSPI_CCR_DMODE_1_LINE |
        QUADSPI_CCR_IDMOD_1_LINE | READ_STATUS_REG_CMD;

    stm32_quadspi_wait_flag(QUADSPI_SR_SMF);

    /* Enter 4-bytes address mode */
    stm32_quadspi_write_enable();

    stm32_quadspi_busy_wait();

    *QUADSPI_CCR = QUADSPI_CCR_FMODE_IND_WR | QUADSPI_CCR_IDMOD_1_LINE |
        ENTER_4_BYTE_ADDR_MODE_CMD;

    stm32_quadspi_wait_flag(QUADSPI_SR_TCF);

    stm32_quadspi_busy_wait();

    *QUADSPI_PSMAR = 0;
    *QUADSPI_PSMKR = S25FL512S_SR_WIP;
    *QUADSPI_PIR = 0x10;

    *QUADSPI_CR |= QUADSPI_CR_AMPS;
    *QUADSPI_DLR = 0;
    *QUADSPI_CCR = QUADSPI_CCR_FMODE_AUTO_POLL | QUADSPI_CCR_DMODE_1_LINE |
        QUADSPI_CCR_IDMOD_1_LINE | READ_STATUS_REG_CMD;

    stm32_quadspi_wait_flag(QUADSPI_SR_SMF);

    /* Configure dummy cycles on memory side */

    stm32_quadspi_busy_wait();

    *QUADSPI_DLR = 0;
    *QUADSPI_CCR = QUADSPI_CCR_FMODE_IND_WR | QUADSPI_CCR_DMODE_1_LINE |
        QUADSPI_CCR_IDMOD_1_LINE | READ_VOL_CFG_REG_CMD;

    *QUADSPI_CCR |= QUADSPI_CCR_FMODE_IND_RD;

    *QUADSPI_AR = *QUADSPI_AR; //Needed?

    while (!(*QUADSPI_SR & (QUADSPI_SR_FTF | QUADSPI_SR_TCF)));

    reg = *QUADSPI_DR;

    stm32_quadspi_wait_flag(QUADSPI_SR_TCF);

    reg = (reg & ~0xf0) | (10 /*params->dummy_cycle*/ << 4);

    stm32_quadspi_write_enable();

    stm32_quadspi_busy_wait();

    *QUADSPI_DLR = 0;
    *QUADSPI_CCR = QUADSPI_CCR_FMODE_IND_WR | QUADSPI_CCR_DMODE_1_LINE |
        QUADSPI_CCR_IDMOD_1_LINE | WRITE_VOL_CFG_REG_CMD;

    while (!(*QUADSPI_SR & QUADSPI_SR_FTF));

    *QUADSPI_DR = reg;

    stm32_quadspi_wait_flag(QUADSPI_SR_TCF);

    stm32_quadspi_busy_wait();

    *QUADSPI_CCR = QUADSPI_CCR_FMODE_MEMMAP | QUADSPI_CCR_DMODE_4_LINES |
        QUADSPI_CCR_DCYC(10 /*params->dummy_cycle*/) | QUADSPI_CCR_ADSIZE_32BITS /*params->address_size*/ |
        QUADSPI_CCR_ADMOD_1_LINE | QUADSPI_CCR_IDMOD_1_LINE |
        QUAD_OUTPUT_FAST_READ_CMD;

    stm32_quadspi_busy_wait();
}

