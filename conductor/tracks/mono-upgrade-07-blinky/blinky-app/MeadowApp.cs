// Blinky for F7CoreComputeV2
// PA0 = D20 = Blue LED on dev board (active LOW)

using System;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;

public class MeadowApp : App<F7CoreComputeV2>
{
    IDigitalOutputPort led;

    public override Task Initialize()
    {
        Console.WriteLine("BlinkyCS: Initialize (F7CoreComputeV2)");

        led = Device.CreateDigitalOutputPort(Device.Pins.D20, false);
        Console.WriteLine("BlinkyCS: LED port created (D20 = PA0)");

        return Task.CompletedTask;
    }

    public override async Task Run()
    {
        Console.WriteLine("BlinkyCS: Run — starting blink loop");

        for (int i = 0; i < 10; i++)
        {
            led.State = true;
            Console.WriteLine($"  [{i}] LED ON");
            await Task.Delay(500);
            led.State = false;
            Console.WriteLine($"  [{i}] LED OFF");
            await Task.Delay(500);
        }

        Console.WriteLine("BlinkyCS: Blink complete!");
    }
}
