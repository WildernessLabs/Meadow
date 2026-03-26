// GPIO Test — Phase 2 of Track 07
// Toggles LED_R (PA2) via the NuttX UPD driver ioctl interface.
// No Meadow.Core dependency — raw P/Invoke to open/ioctl/close.

using System;
using System.Runtime.InteropServices;
using System.Threading;

class Program
{
    // P/Invoke to NuttX via mappings-meadow.h
    [DllImport("nuttx")]
    static extern int open(string path, int flags);

    [DllImport("nuttx")]
    static extern int close(int fd);

    [DllImport("nuttx")]
    static extern unsafe int ioctl(int fd, int request, void* arg);

    // UPD ioctl commands (from stm32f777zit6-meadow.h)
    const int MUPD_SET_REGISTER    = 1;
    const int MUPD_GET_REGISTER    = 2;
    const int MUPD_UPDATE_REGISTER = 3;

    // open() flags
    const int O_RDONLY = 0;
    const int O_WRONLY = 1;
    const int O_RDWR = 2;

    // STM32F7 GPIO register addresses
    // GPIOA base = 0x40020000
    const uint GPIOA_BASE  = 0x40020000;
    const uint GPIO_MODER  = 0x00;  // Mode register
    const uint GPIO_OTYPER = 0x04;  // Output type register
    const uint GPIO_OSPEED = 0x08;  // Output speed register
    const uint GPIO_PUPDR  = 0x0C;  // Pull-up/pull-down register
    const uint GPIO_IDR    = 0x10;  // Input data register
    const uint GPIO_BSRR   = 0x18;  // Bit set/reset register

    // LED_R = PA2 (pin 2 on port A)
    const int LED_PIN = 2;

    // Structs must match native layout (upd_register_value / upd_register_update)
    struct UpdRegisterValue
    {
        public uint Address;
        public uint Value;
    }

    struct UpdRegisterUpdate
    {
        public uint Address;
        public uint ClearBits;
        public uint SetBits;
    }

    static unsafe bool SetRegister(int fd, uint address, uint value)
    {
        var reg = new UpdRegisterValue { Address = address, Value = value };
        return ioctl(fd, MUPD_SET_REGISTER, &reg) == 0;
    }

    static unsafe bool GetRegister(int fd, uint address, out uint value)
    {
        var reg = new UpdRegisterValue { Address = address, Value = 0 };
        int ret = ioctl(fd, MUPD_GET_REGISTER, &reg);
        value = reg.Value;
        return ret == 0;
    }

    static unsafe bool UpdateRegister(int fd, uint address, uint clearBits, uint setBits)
    {
        var upd = new UpdRegisterUpdate
        {
            Address = address,
            ClearBits = clearBits,
            SetBits = setBits
        };
        return ioctl(fd, MUPD_UPDATE_REGISTER, &upd) == 0;
    }

    static int Main()
    {
        Console.WriteLine("GPIO test start — toggling LED_R (PA2)");

        // Open the UPD driver
        int fd = open("/dev/upd", O_RDONLY);
        if (fd < 0)
        {
            Console.WriteLine($"ERROR: open(/dev/upd) failed, fd={fd}");
            return 1;
        }
        Console.WriteLine($"UPD driver opened, fd={fd}");

        // Read current MODER value for diagnostics
        if (GetRegister(fd, GPIOA_BASE + GPIO_MODER, out uint moder))
            Console.WriteLine($"GPIOA_MODER = 0x{moder:X8}");

        // Configure PA2 as General Purpose Output (MODER bits [5:4] = 01)
        // Clear bits [5:4] (mask = 0x30), set bit 4 (value = 0x10)
        uint moderClear = (uint)(0x3 << (LED_PIN * 2));  // 0x30
        uint moderSet   = (uint)(0x1 << (LED_PIN * 2));  // 0x10
        if (!UpdateRegister(fd, GPIOA_BASE + GPIO_MODER, moderClear, moderSet))
        {
            Console.WriteLine("ERROR: failed to configure MODER");
            close(fd);
            return 2;
        }

        // Set push-pull output type (OTYPER bit 2 = 0)
        if (!UpdateRegister(fd, GPIOA_BASE + GPIO_OTYPER, (uint)(1 << LED_PIN), 0))
        {
            Console.WriteLine("ERROR: failed to configure OTYPER");
            close(fd);
            return 3;
        }

        // Set medium speed (OSPEED bits [5:4] = 01)
        uint speedClear = (uint)(0x3 << (LED_PIN * 2));
        uint speedSet   = (uint)(0x1 << (LED_PIN * 2));
        if (!UpdateRegister(fd, GPIOA_BASE + GPIO_OSPEED, speedClear, speedSet))
        {
            Console.WriteLine("ERROR: failed to configure OSPEED");
            close(fd);
            return 4;
        }

        // No pull-up/pull-down (PUPDR bits [5:4] = 00)
        if (!UpdateRegister(fd, GPIOA_BASE + GPIO_PUPDR, (uint)(0x3 << (LED_PIN * 2)), 0))
        {
            Console.WriteLine("ERROR: failed to configure PUPDR");
            close(fd);
            return 5;
        }

        // Read back MODER to confirm
        if (GetRegister(fd, GPIOA_BASE + GPIO_MODER, out uint moderAfter))
            Console.WriteLine($"GPIOA_MODER after config = 0x{moderAfter:X8}");

        Console.WriteLine("PA2 configured as output — starting blink loop");

        // Blink 10 times: set high (BSRR bit 2), wait, set low (BSRR bit 18), wait
        for (int i = 0; i < 10; i++)
        {
            // Set PA2 high: write to BSRR[2] (set bit)
            SetRegister(fd, GPIOA_BASE + GPIO_BSRR, (uint)(1 << LED_PIN));
            Console.WriteLine($"  [{i}] LED ON");
            Thread.Sleep(500);

            // Set PA2 low: write to BSRR[18] (reset bit = bit 16 + pin)
            SetRegister(fd, GPIOA_BASE + GPIO_BSRR, (uint)(1 << (LED_PIN + 16)));
            Console.WriteLine($"  [{i}] LED OFF");
            Thread.Sleep(500);
        }

        Console.WriteLine("Blink complete — reading final IDR");
        if (GetRegister(fd, GPIOA_BASE + GPIO_IDR, out uint idr))
            Console.WriteLine($"GPIOA_IDR = 0x{idr:X8}");

        close(fd);
        Console.WriteLine("GPIO test done");
        return 42;
    }
}
