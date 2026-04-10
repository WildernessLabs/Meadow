// Test: DNS + sockets + HttpClient (cold/warm) + HTTPS over WiFi
using System;
using System.Diagnostics;
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

        Console.WriteLine("Waiting 15s for network...");
        await Task.Delay(15000);

        // Test 1: DNS
        Console.WriteLine("=== TEST 1: DNS ===");
        IPAddress serverIp = null;
        try
        {
            var sw = Stopwatch.StartNew();
            var addresses = Dns.GetHostAddresses("example.com");
            sw.Stop();
            foreach (var addr in addresses)
                Console.WriteLine($"  example.com -> {addr}");
            if (addresses.Length > 0)
                serverIp = addresses[0];
            Console.WriteLine($"DNS OK ({sw.ElapsedMilliseconds}ms)");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"DNS FAIL: {ex.GetType().Name}: {ex.Message}");
        }

        // Test 2: Raw sync socket
        if (serverIp != null)
        {
            Console.WriteLine("=== TEST 2: RAW SOCKET ===");
            try
            {
                var sw = Stopwatch.StartNew();
                using var sock = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                sock.Connect(new IPEndPoint(serverIp, 80));
                var sent = sock.Send(Encoding.ASCII.GetBytes("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n"));
                var buf = new byte[4096];
                var total = 0;
                int n;
                while ((n = sock.Receive(buf)) > 0)
                    total += n;
                sw.Stop();
                Console.WriteLine($"RAW SOCKET OK: {total} bytes in {sw.ElapsedMilliseconds}ms");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"RAW SOCKET FAIL: {ex.GetType().Name}: {ex.Message}");
            }
        }

        // Test 3: Async socket
        if (serverIp != null)
        {
            Console.WriteLine("=== TEST 3: ASYNC SOCKET ===");
            try
            {
                var sw = Stopwatch.StartNew();
                using var sock = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                await sock.ConnectAsync(new IPEndPoint(serverIp, 80));
                await sock.SendAsync(Encoding.ASCII.GetBytes("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n"), SocketFlags.None);
                var buf = new byte[4096];
                var total = 0;
                int n;
                while ((n = await sock.ReceiveAsync(buf, SocketFlags.None)) > 0)
                    total += n;
                sw.Stop();
                Console.WriteLine($"ASYNC SOCKET OK: {total} bytes in {sw.ElapsedMilliseconds}ms");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ASYNC SOCKET FAIL: {ex.GetType().Name}: {ex.Message}");
            }
        }

        // Test 4: HttpClient COLD (first request — JIT warmup)
        Console.WriteLine("=== TEST 4: HTTP GET (cold) ===");
        try
        {
            var sw = Stopwatch.StartNew();
            using var client = new HttpClient();
            client.Timeout = TimeSpan.FromSeconds(60);
            var request = new HttpRequestMessage(HttpMethod.Get, "http://example.com");
            request.Version = new Version(1, 0);
            var response = await client.SendAsync(request);
            var body = await response.Content.ReadAsStringAsync();
            sw.Stop();
            Console.WriteLine($"HTTP COLD OK: {(int)response.StatusCode} {response.ReasonPhrase}, {body.Length} chars in {sw.ElapsedMilliseconds}ms");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"HTTP COLD FAIL: {ex.GetType().Name}: {ex.Message}");
            if (ex.InnerException != null)
                Console.WriteLine($"  Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
        }

        // Test 5: HttpClient WARM (second request — JIT already done)
        Console.WriteLine("=== TEST 5: HTTP GET (warm) ===");
        try
        {
            var sw = Stopwatch.StartNew();
            using var client = new HttpClient();
            client.Timeout = TimeSpan.FromSeconds(30);
            var request = new HttpRequestMessage(HttpMethod.Get, "http://example.com");
            request.Version = new Version(1, 0);
            var response = await client.SendAsync(request);
            var body = await response.Content.ReadAsStringAsync();
            sw.Stop();
            Console.WriteLine($"HTTP WARM OK: {(int)response.StatusCode} {response.ReasonPhrase}, {body.Length} chars in {sw.ElapsedMilliseconds}ms");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"HTTP WARM FAIL: {ex.GetType().Name}: {ex.Message}");
            if (ex.InnerException != null)
                Console.WriteLine($"  Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
        }

        // Test 6: HTTPS GET (TLS via mbedtls)
        Console.WriteLine("=== TEST 6: HTTPS GET ===");
        try
        {
            var sw = Stopwatch.StartNew();
            using var client = new HttpClient();
            client.Timeout = TimeSpan.FromSeconds(60);
            var request = new HttpRequestMessage(HttpMethod.Get, "https://example.com");
            request.Version = new Version(1, 0);
            var response = await client.SendAsync(request);
            var body = await response.Content.ReadAsStringAsync();
            sw.Stop();
            Console.WriteLine($"HTTPS OK: {(int)response.StatusCode} {response.ReasonPhrase}, {body.Length} chars in {sw.ElapsedMilliseconds}ms");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"HTTPS FAIL: {ex.GetType().Name}: {ex.Message}");
            if (ex.InnerException != null)
                Console.WriteLine($"  Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
            var inner2 = ex.InnerException?.InnerException;
            if (inner2 != null)
                Console.WriteLine($"  Inner2: {inner2.GetType().Name}: {inner2.Message}");
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
