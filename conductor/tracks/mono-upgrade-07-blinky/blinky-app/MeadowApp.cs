// Blinky — Emulator: F7FeatherV2, Hardware: F7CoreComputeV2
// Change device type below to match target platform.
// F7FeatherV2: LED on PA2 (active LOW)
// F7CoreComputeV2: LEDs PA0=Blue, PA1=Green, PA2=Red (active LOW)

using System;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;

public class MeadowApp : App<F7FeatherV2>
{
    IDigitalOutputPort ledR;

    public override Task Initialize()
    {
        Console.WriteLine("BlinkyCS: Initialize (F7FeatherV2)");

        ledR = Device.CreateDigitalOutputPort(Device.Pins.OnboardLedRed, false);
        Console.WriteLine("BlinkyCS: LED port created (OnboardLedRed = PA2)");

        return Task.CompletedTask;
    }

    public override async Task Run()
    {
        Console.WriteLine("BlinkyCS: Run — starting blink loop");

        for (int i = 0; i < 10; i++)
        {
            ledR.State = true;
            Console.WriteLine($"  [{i}] LED ON");
            await Task.Delay(500);
            ledR.State = false;
            Console.WriteLine($"  [{i}] LED OFF");
            await Task.Delay(500);
        }

        Console.WriteLine("BlinkyCS: Blink complete!");
    }
}
