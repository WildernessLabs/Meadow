// Blinky — Phase 4 of Track 07
// Full Meadow stack: App<F7FeatherV2> → DigitalOutputPort → UPD driver → GPIO
// MeadowOS.Main (in Meadow.dll) is the entry point; this DLL is discovered via reflection.

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
        Console.WriteLine("BlinkyCS: Initialize");

        // LED_R is PA2 — onboard red LED, active LOW (inverse logic on F7 Feather)
        ledR = Device.CreateDigitalOutputPort(Device.Pins.OnboardLedRed, false);
        Console.WriteLine("BlinkyCS: LED port created");

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

        Console.WriteLine("BlinkyCS: Blink complete");
    }
}
