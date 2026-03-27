// Blinky — Track 08 Hardware Validation
// Full Meadow stack: App<F7CoreComputeV2> on real hardware
// LEDs: PA0=Blue, PA1=Green, PA2=Red (active LOW on CoreCompute)

using System;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;

public class MeadowApp : App<F7CoreComputeV2>
{
    IDigitalOutputPort ledR, ledG, ledB;

    public override Task Initialize()
    {
        Console.WriteLine("BlinkyCS: Initialize (F7CoreComputeV2)");

        ledB = Device.CreateDigitalOutputPort(Device.Pins.PA0, false);
        ledG = Device.CreateDigitalOutputPort(Device.Pins.PA1_ETH_REF_CLK, false);
        ledR = Device.CreateDigitalOutputPort(Device.Pins.PA2_ETH_MDIO, false);
        Console.WriteLine("BlinkyCS: LED ports created (R=PA2, G=PA1, B=PA0)");

        return Task.CompletedTask;
    }

    public override async Task Run()
    {
        Console.WriteLine("BlinkyCS: Run — starting blink loop");

        for (int i = 0; i < 10; i++)
        {
            // Red
            ledR.State = true;
            Console.WriteLine($"  [{i}] RED");
            await Task.Delay(300);
            ledR.State = false;

            // Green
            ledG.State = true;
            Console.WriteLine($"  [{i}] GREEN");
            await Task.Delay(300);
            ledG.State = false;

            // Blue
            ledB.State = true;
            Console.WriteLine($"  [{i}] BLUE");
            await Task.Delay(300);
            ledB.State = false;

            await Task.Delay(100);
        }

        Console.WriteLine("BlinkyCS: Blink complete!");
    }
}
