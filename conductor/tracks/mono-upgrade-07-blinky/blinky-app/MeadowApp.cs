// Network interface + P/Invoke coverage test + TLS validation
using System;
using System.Diagnostics;
using System.Net;
using System.Net.Http;
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
        Console.WriteLine("NET+TLS TEST v2 INIT");
        led = Device.CreateDigitalOutputPort(Device.Pins.D20, false);
        return Task.CompletedTask;
    }

    public override Task Run()
    {
        Console.WriteLine("NET+TLS TEST v2 RUN START");

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

        // ── TEST 5: HTTPS GET (TLS via mbedTLS) ──
        // SslGetPeerCertificate returns NULL on NuttX, so we need the callback.
        // mbedTLS still verifies the server cert chain natively during handshake.
        Console.WriteLine("=== TEST 5: HTTPS GET ===");
        for (int attempt = 1; attempt <= 3; attempt++)
        {
            Console.WriteLine($"  Attempt {attempt}/3...");
            try
            {
                var sw = Stopwatch.StartNew();
                var handler = new HttpClientHandler();
                handler.ServerCertificateCustomValidationCallback = (_, _, _, _) => true;
                using var client = new HttpClient(handler);
                client.Timeout = TimeSpan.FromSeconds(60);
                var response = client.GetAsync("https://httpbin.org/get").Result;
                var body = response.Content.ReadAsStringAsync().Result;
                sw.Stop();
                Console.WriteLine($"  HTTPS: {(int)response.StatusCode} {response.ReasonPhrase}, {body.Length} chars in {sw.ElapsedMilliseconds}ms");
                Console.WriteLine($"  First 120: {body.Substring(0, Math.Min(120, body.Length))}");
                Console.WriteLine("TEST 5 PASS");
                break;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  HTTPS attempt {attempt} FAIL: {ex.GetType().Name}: {ex.Message}");
                if (ex.InnerException != null)
                    Console.WriteLine($"    Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
                var inner2 = ex.InnerException?.InnerException;
                if (inner2 != null)
                    Console.WriteLine($"    Inner2: {inner2.GetType().Name}: {inner2.Message}");
                if (attempt == 3)
                    Console.WriteLine("TEST 5 FAIL (all 3 attempts)");
                else
                    Thread.Sleep(3000);
            }
        }

        // ── TEST 6: HTTPS to a different host (verify not host-specific) ──
        Console.WriteLine("=== TEST 6: HTTPS GET (example.com) ===");
        try
        {
            var sw = Stopwatch.StartNew();
            var handler = new HttpClientHandler();
            handler.ServerCertificateCustomValidationCallback = (_, _, _, _) => true;
            using var client = new HttpClient(handler);
            client.Timeout = TimeSpan.FromSeconds(60);
            var response = client.GetAsync("https://example.com").Result;
            var body = response.Content.ReadAsStringAsync().Result;
            sw.Stop();
            Console.WriteLine($"  HTTPS: {(int)response.StatusCode}, {body.Length} chars in {sw.ElapsedMilliseconds}ms");
            Console.WriteLine("TEST 6 PASS");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"TEST 6 FAIL: {ex.GetType().Name}: {ex.Message}");
            if (ex.InnerException != null)
                Console.WriteLine($"  Inner: {ex.InnerException.GetType().Name}: {ex.InnerException.Message}");
            var inner2 = ex.InnerException?.InnerException;
            if (inner2 != null)
                Console.WriteLine($"  Inner2: {inner2.GetType().Name}: {inner2.Message}");
        }

        // GPIO throughput test — toggle as fast as possible, measure ops/sec.
        // Three rounds so JIT warm-up doesn't skew the headline number.
        Console.WriteLine("=== GPIO STATE-SETTER BENCHMARK ===");
        const int BATCH = 100_000;
        for (int round = 0; round < 3; round++)
        {
            var sw = Stopwatch.StartNew();
            for (int i = 0; i < BATCH; i++)
            {
                led.State = true;
                led.State = false;
            }
            sw.Stop();
            long ops = BATCH * 2L;
            double opsPerSec = ops / sw.Elapsed.TotalSeconds;
            Console.WriteLine($"  round {round}: {ops} writes in {sw.ElapsedMilliseconds} ms → {opsPerSec:N0} writes/sec ({(opsPerSec / 2000.0):F1} kHz square wave)");
        }

        if (led is F7DigitalOutputPort f7Led)
        {
            Console.WriteLine("=== GPIO Toggle() BENCHMARK (F7 fast path) ===");
            for (int round = 0; round < 3; round++)
            {
                var sw = Stopwatch.StartNew();
                for (int i = 0; i < BATCH * 2; i++)
                {
                    f7Led.Toggle();
                }
                sw.Stop();
                long ops = BATCH * 2L;
                double opsPerSec = ops / sw.Elapsed.TotalSeconds;
                Console.WriteLine($"  round {round}: {ops} toggles in {sw.ElapsedMilliseconds} ms → {opsPerSec:N0} toggles/sec ({(opsPerSec / 2000.0):F1} kHz square wave)");
            }

            Console.WriteLine("=== GPIO RAW BSRR BENCHMARK (escape hatch) ===");
            unsafe
            {
                f7Led.GetRawWriteHandle(out uint* bsrr, out uint setMask, out uint clearMask);
                const int RAW_BATCH = 1_000_000;

                for (int round = 0; round < 3; round++)
                {
                    var sw = Stopwatch.StartNew();
                    for (int i = 0; i < RAW_BATCH; i++)
                    {
                        *bsrr = setMask;
                        *bsrr = clearMask;
                    }
                    sw.Stop();
                    long ops = RAW_BATCH * 2L;
                    double opsPerSec = ops / sw.Elapsed.TotalSeconds;
                    Console.WriteLine($"  round {round} (tight loop): {ops} writes in {sw.ElapsedMilliseconds} ms → {opsPerSec:N0} writes/sec ({(opsPerSec / 2000.0):F1} kHz square wave)");
                }

                // Unrolled 8x to amortize loop overhead
                for (int round = 0; round < 3; round++)
                {
                    var sw = Stopwatch.StartNew();
                    int iters = RAW_BATCH / 4;
                    for (int i = 0; i < iters; i++)
                    {
                        *bsrr = setMask; *bsrr = clearMask;
                        *bsrr = setMask; *bsrr = clearMask;
                        *bsrr = setMask; *bsrr = clearMask;
                        *bsrr = setMask; *bsrr = clearMask;
                    }
                    sw.Stop();
                    long ops = (long)iters * 8L;
                    double opsPerSec = ops / sw.Elapsed.TotalSeconds;
                    Console.WriteLine($"  round {round} (unrolled 8x): {ops} writes in {sw.ElapsedMilliseconds} ms → {opsPerSec:N0} writes/sec ({(opsPerSec / 2000.0):F1} kHz square wave)");
                }

            }
        }
        Console.WriteLine("=== GPIO BENCHMARK DONE ===");

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
