// Test 27: Socket create + close with ToFileDescriptor fix
using System;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;

public class MeadowApp : App<F7CoreComputeV2>
{
    IDigitalOutputPort led;

    public override Task Initialize()
    {
        Console.WriteLine("Init");
        led = Device.CreateDigitalOutputPort(Device.Pins.D20, false);
        return Task.CompletedTask;
    }

    public override async Task Run()
    {
        Console.WriteLine("=== Test 27: Socket create + close ===");

        Console.WriteLine(">> Creating socket...");
        Socket s = null;
        try
        {
            s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            Console.WriteLine($"   OK! handle={s.Handle}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"   CREATE THREW: {ex.GetType().Name}: {ex.Message}");
        }

        Console.WriteLine(">> Closing socket...");
        try
        {
            s?.Close();
            Console.WriteLine("   Close OK!");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"   CLOSE THREW: {ex.GetType().Name}: {ex.Message}");
        }

        Console.WriteLine(">> Socket lifecycle complete!");
        Console.WriteLine(">> Creating second socket...");
        try
        {
            using var s2 = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            Console.WriteLine($"   OK! handle={s2.Handle}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"   THREW: {ex.GetType().Name}: {ex.Message}");
        }
        Console.WriteLine("   Second socket done (using disposed)");

        Console.WriteLine(">> Entering heartbeat...");
        int cycle = 0;
        while (true)
        {
            led.State = !led.State;
            Thread.Sleep(1000);
            cycle++;
            if (cycle % 10 == 0)
                Console.WriteLine($"  [heartbeat {cycle}]");
        }
    }
}
