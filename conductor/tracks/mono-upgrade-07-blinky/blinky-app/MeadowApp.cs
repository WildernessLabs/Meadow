// Network interface + P/Invoke coverage test
using System;
using System.Net;
using System.Net.NetworkInformation;
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
        Console.WriteLine("NET-IFACE TEST v1 INIT");
        led = Device.CreateDigitalOutputPort(Device.Pins.D20, false);
        return Task.CompletedTask;
    }

    public override Task Run()
    {
        Console.WriteLine("NET-IFACE TEST v1 RUN START");

        // ── TEST 1: NetworkInterface.GetAllNetworkInterfaces ──
        Console.WriteLine("=== TEST 1: GetAllNetworkInterfaces ===");
        try
        {
            var interfaces = NetworkInterface.GetAllNetworkInterfaces();
            Console.WriteLine($"  Found {interfaces.Length} interface(s)");
            foreach (var ni in interfaces)
            {
                Console.WriteLine($"  [{ni.Name}] type={ni.NetworkInterfaceType} status={ni.OperationalStatus}");
                try
                {
                    var props = ni.GetIPProperties();
                    foreach (var addr in props.UnicastAddresses)
                    {
                        Console.WriteLine($"    IP: {addr.Address}");
                    }
                }
                catch (Exception innerEx)
                {
                    Console.WriteLine($"    GetIPProperties failed: {innerEx.GetType().Name}: {innerEx.Message}");
                }
            }
            Console.WriteLine("TEST 1 PASS");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"TEST 1 FAIL: {ex.GetType().Name}: {ex.Message}");
            if (ex.InnerException != null)
                Console.WriteLine($"  Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
            Console.WriteLine($"  Stack: {ex.StackTrace}");
        }

        // ── TEST 2: DNS resolution ──
        Console.WriteLine("=== TEST 2: DNS ===");
        try
        {
            var addrs = Dns.GetHostAddresses("example.com");
            Console.WriteLine($"  Resolved {addrs.Length} address(es)");
            foreach (var a in addrs)
            {
                Console.WriteLine($"    {a}");
            }
            Console.WriteLine("TEST 2 PASS");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"TEST 2 FAIL: {ex.GetType().Name}: {ex.Message}");
        }

        // ── TEST 3: GC.Collect (regression check) ──
        Console.WriteLine("=== TEST 3: GC.Collect ===");
        try
        {
            for (int i = 0; i < 5; i++)
            {
                var tmp = new byte[4096];
                tmp[0] = (byte)i;
            }
            GC.Collect();
            Console.WriteLine($"  Gen0={GC.CollectionCount(0)} TotalMem={GC.GetTotalMemory(false)}");
            Console.WriteLine("TEST 3 PASS");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"TEST 3 FAIL: {ex.GetType().Name}: {ex.Message}");
        }

        // Wait for WiFi to be ready
        Console.WriteLine("Waiting 15s for WiFi...");
        Thread.Sleep(15000);

        // ── TEST 4: HTTP GET (plain) ──
        Console.WriteLine("=== TEST 4: HTTP GET ===");
        try
        {
            using var client = new System.Net.Http.HttpClient();
            client.Timeout = TimeSpan.FromSeconds(30);
            var body = client.GetStringAsync("http://example.com").Result;
            Console.WriteLine($"  Got {body.Length} chars");
            Console.WriteLine($"  First 80: {body.Substring(0, Math.Min(80, body.Length))}");
            Console.WriteLine("TEST 4 PASS");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"TEST 4 FAIL: {ex.GetType().Name}: {ex.Message}");
            if (ex.InnerException != null)
                Console.WriteLine($"  Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
        }

        // Heartbeat
        Console.WriteLine("=== ALL TESTS DONE ===");
        int c = 0;
        while (true)
        {
            led.State = !led.State;
            Console.WriteLine($"BEAT {c++}");
            Thread.Sleep(2000);
        }
    }
}
