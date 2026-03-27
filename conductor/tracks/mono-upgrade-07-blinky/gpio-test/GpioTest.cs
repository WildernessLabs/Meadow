// Direct GPIO blink test — Phase 2 of Track 07
// Bypasses MeadowOS/Meadow.Core entirely.
// P/Invokes directly into NuttX stm32_configgpio / stm32_gpiowrite
// to toggle LED_R (PA2) on F7 Feather V2.

using System;
using System.Runtime.InteropServices;
using System.Threading;

class GpioTest
{
    // NuttX STM32 GPIO functions (use int for uint32_t — same size, avoids issues)
    [DllImport("nuttx")]
    static extern int stm32_configgpio(int cfgset);

    [DllImport("nuttx")]
    static extern void stm32_gpiowrite(int pinset, int value);

    // GPIO bit encoding for STM32F7/H7 (from stm32_gpio.h):
    //   Bits 18-19: Mode (1=output)
    //   Bits 10-11: Speed (2=50MHz)
    //   Bits 4-7:   Port (0=A)
    //   Bits 0-3:   Pin (2)
    // PA2 output push-pull, 50MHz = 0x40802
    const int PA2_OUTPUT = (1 << 18) | (2 << 10) | (0 << 4) | 2;

    static int Main()
    {
        try { Console.WriteLine("GPIO blink test start"); } catch { }

        // Configure PA2 as output
        int ret = stm32_configgpio(PA2_OUTPUT);
        try { Console.WriteLine("stm32_configgpio(PA2) => " + ret); } catch { }

        // Blink loop: toggle PA2 (LED_R is active-low on F7 Feather)
        for (int i = 0; i < 10; i++)
        {
            stm32_gpiowrite(PA2_OUTPUT, 0);  // LED ON (active-low)
            try { Console.WriteLine("  LED ON " + i); } catch { }
            Thread.Sleep(500);

            stm32_gpiowrite(PA2_OUTPUT, 1);  // LED OFF
            try { Console.WriteLine("  LED OFF " + i); } catch { }
            Thread.Sleep(500);
        }

        try { Console.WriteLine("GPIO blink test complete"); } catch { }
        return 42;
    }
}
