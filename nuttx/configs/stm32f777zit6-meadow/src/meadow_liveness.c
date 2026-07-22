/****************************************************************************
 * configs/stm32f777zit6-meadow/src/meadow_liveness.c
 *
 * Managed-runtime liveness pet for the IWDG backstop.
 *
 * The IWDG is kicked from the SysTick handler, so it only covers wedges
 * that silence the kernel clock.  The observed "managed silence" wedge
 * leaves the OS perfectly healthy while every .NET thread parks forever
 * (timer machinery dead) -- the device looks hung to the user but no
 * watchdog fires.
 *
 * This driver closes that gap: managed code writes /dev/liveness from a
 * System.Threading.Timer (Meadow.Core data-forwarder, 30s period), which
 * stamps g_meadow_liveness_last_tick.  Once the first pet arrives the
 * backstop is ARMED: if pets stop for MEADOW_LIVENESS_TIMEOUT_TICKS the
 * SysTick handler stops kicking the IWDG and the device hard-resets ~32s
 * later (see stm32_timerisr.c).  Apps that never pet (no Meadow.Core cloud
 * stack, runtime disabled) never arm it, preserving pure-OS-backstop
 * behavior.
 *
 * Writing the single character 'D' DISARMS the gate (used when Mono is
 * intentionally stopped, e.g. runtime disable / app deploy).
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/fs/fs.h>
#include <nuttx/clock.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>

/****************************************************************************
 * Public Data (read by the SysTick IWDG kicker in stm32_timerisr.c)
 ****************************************************************************/

volatile uint32_t g_meadow_liveness_last_tick;
volatile int g_meadow_liveness_armed;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static ssize_t liveness_write(FAR struct file *filep, FAR const char *buffer,
                              size_t buflen)
{
  if (buflen > 0 && buffer != NULL && buffer[0] == 'D')
    {
      if (g_meadow_liveness_armed)
        {
          syslog(LOG_NOTICE, "liveness: disarmed (mono stopping)\n");
        }

      g_meadow_liveness_armed = 0;
      return (ssize_t)buflen;
    }

  g_meadow_liveness_last_tick = (uint32_t)clock_systimer();
  if (!g_meadow_liveness_armed)
    {
      g_meadow_liveness_armed = 1;
      syslog(LOG_NOTICE, "liveness: armed (managed heartbeat active)\n");
    }

  return (ssize_t)buflen;
}

static ssize_t liveness_read(FAR struct file *filep, FAR char *buffer,
                             size_t buflen)
{
  return 0; /* EOF; state is observable via syslog / SWD */
}

static const struct file_operations g_liveness_ops =
{
  .read  = liveness_read,
  .write = liveness_write
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void meadow_liveness_disarm(void)
{
  if (g_meadow_liveness_armed)
    {
      syslog(LOG_NOTICE, "liveness: disarmed (mono stopping)\n");
    }

  g_meadow_liveness_armed = 0;
}

int meadow_liveness_initialize(void)
{
  return register_driver("/dev/liveness", &g_liveness_ops, 0666, NULL);
}
