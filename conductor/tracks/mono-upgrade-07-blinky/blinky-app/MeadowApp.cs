// Test: DNS + raw socket + HttpClient over WiFi
using System;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;

public class MeadowApp : App<F7CoreComputeV2>
{
    IDigitalOutputPort led;

    public override Task Initialize()
    {
        Console.WriteLine("NET TEST INIT");
        led = Device.CreateDigitalOutputPort(Device.Pins.D20, false);
        return Task.CompletedTask;
    }

    public override async Task Run()
    {
        Console.WriteLine("NET TEST RUN START");

        // Give WiFi/DHCP time to settle
        Console.WriteLine("Waiting 15s for network...");
        await Task.Delay(15000);

        // Test 1: DNS resolution
        Console.WriteLine("=== TEST 1: DNS ===");
        IPAddress serverIp = null;
        try
        {
            var addresses = Dns.GetHostAddresses("example.com");
            foreach (var addr in addresses)
                Console.WriteLine($"  example.com -> {addr}");
            if (addresses.Length > 0)
                serverIp = addresses[0];
            Console.WriteLine("DNS OK");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"DNS FAIL: {ex.GetType().Name}: {ex.Message}");
        }

        // Test 2: Raw socket connect + HTTP manually
        if (serverIp != null)
        {
            Console.WriteLine("=== TEST 2: RAW SOCKET ===");
            try
            {
                Console.WriteLine("  Creating socket...");
                using var sock = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                Console.WriteLine($"  Socket created: {sock.Handle}");

                Console.WriteLine($"  Connecting to {serverIp}:80...");
                sock.Connect(new IPEndPoint(serverIp, 80));
                Console.WriteLine("  Connected!");

                var request = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";
                Console.WriteLine("  Sending request...");
                var sent = sock.Send(Encoding.ASCII.GetBytes(request));
                Console.WriteLine($"  Sent {sent} bytes");

                Console.WriteLine("  Receiving response...");
                var buf = new byte[4096];
                var total = 0;
                int n;
                while ((n = sock.Receive(buf)) > 0)
                {
                    total += n;
                    if (total <= 200)
                        Console.WriteLine($"  [{n} bytes]: {Encoding.ASCII.GetString(buf, 0, Math.Min(n, 200))}");
                    else
                        Console.WriteLine($"  [{n} bytes] (total: {total})");
                }
                Console.WriteLine($"  Total received: {total} bytes");
                Console.WriteLine("RAW SOCKET OK");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"RAW SOCKET FAIL: {ex.GetType().Name}: {ex.Message}");
                if (ex.InnerException != null)
                    Console.WriteLine($"  Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
            }
        }

        // Test 3: Async socket connect
        if (serverIp != null)
        {
            Console.WriteLine("=== TEST 3: ASYNC SOCKET ===");
            try
            {
                Console.WriteLine("  Creating socket...");
                using var sock = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                Console.WriteLine($"  Socket created: {sock.Handle}");

                Console.WriteLine($"  ConnectAsync to {serverIp}:80...");
                var cts = new System.Threading.CancellationTokenSource(TimeSpan.FromSeconds(15));
                await sock.ConnectAsync(new IPEndPoint(serverIp, 80), cts.Token);
                Console.WriteLine("  ConnectAsync succeeded!");

                var request = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";
                Console.WriteLine("  SendAsync...");
                var sent = await sock.SendAsync(Encoding.ASCII.GetBytes(request), SocketFlags.None);
                Console.WriteLine($"  Sent {sent} bytes");

                Console.WriteLine("  ReceiveAsync...");
                var buf = new byte[4096];
                var n = await sock.ReceiveAsync(buf, SocketFlags.None);
                Console.WriteLine($"  Received {n} bytes");
                if (n > 0)
                    Console.WriteLine($"  Preview: {Encoding.ASCII.GetString(buf, 0, Math.Min(n, 200))}");
                Console.WriteLine("ASYNC SOCKET OK");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ASYNC SOCKET FAIL: {ex.GetType().Name}: {ex.Message}");
                if (ex.InnerException != null)
                    Console.WriteLine($"  Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
            }
        }

        // Test 4: HttpClient GET (plain HTTP, HTTP/1.0)
        Console.WriteLine("=== TEST 4: HTTP GET ===");
        try
        {
            Console.WriteLine("  Creating HttpClient...");
            using var client = new HttpClient();
            client.Timeout = TimeSpan.FromSeconds(30);
            Console.WriteLine("  HttpClient created");

            var request = new HttpRequestMessage(HttpMethod.Get, "http://example.com");
            request.Version = new Version(1, 0);
            Console.WriteLine("  Sending request...");

            var response = await client.SendAsync(request);
            Console.WriteLine($"  Status: {(int)response.StatusCode} {response.ReasonPhrase}");

            var body = await response.Content.ReadAsStringAsync();
            Console.WriteLine($"  Body: {body.Length} chars");
            Console.WriteLine("HTTP OK");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"HTTP FAIL: {ex.GetType().Name}: {ex.Message}");
            if (ex.InnerException != null)
                Console.WriteLine($"  Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
        }

        // Heartbeat
        Console.WriteLine("=== HEARTBEAT ===");
        int c = 0;
        while (true)
        {
            led.State = !led.State;
            Console.WriteLine($"BEAT {c++}");
            await Task.Delay(2000);
        }
    }
}
