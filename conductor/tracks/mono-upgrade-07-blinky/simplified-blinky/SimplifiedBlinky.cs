// Simplified Blinky — Phase 4D of Track 07
// Diagnostic: test Assembly reflection (FullName, GetTypes), then GPIO blink.

using System;
using System.IO;
using System.Reflection;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;

class SimplifiedBlinky
{
    static void Log(string msg)
    {
        try { Console.WriteLine(msg); } catch { }
    }

    static int Main()
    {
        Log("[diag] === Assembly Diagnostics ===");

        // Test 0: Self-reflection
        try
        {
            Log("[diag] Test 0: Self-reflection");
            var selfAsm = typeof(SimplifiedBlinky).Assembly;
            Log("[diag]   FullName: " + (selfAsm?.FullName ?? "(null)"));
            var selfTypes = selfAsm.GetTypes();
            Log("[diag]   Types: " + selfTypes.Length + " PASS");
        }
        catch (Exception ex) { Log("[diag]   FAIL: " + ex.GetType().Name + ": " + ex.Message); }

        // Test 1: Enumerate loaded assemblies with FullName
        try
        {
            Log("[diag] Test 1: Loaded assemblies");
            var asms = AppDomain.CurrentDomain.GetAssemblies();
            Log("[diag]   Count: " + asms.Length);
            foreach (var a in asms)
                Log("[diag]     " + (a?.FullName ?? "(null)"));
        }
        catch (Exception ex) { Log("[diag]   FAIL: " + ex.GetType().Name + ": " + ex.Message); }

        // Test 2: GetTypes on Meadow.Contracts (already loaded, no Assembly.Load needed)
        try
        {
            Log("[diag] Test 2: GetTypes on Meadow.Contracts");
            var asm = typeof(Meadow.Hardware.IDigitalOutputPort).Assembly;
            Log("[diag]   FullName: " + (asm?.FullName ?? "(null)"));
            var types = asm.GetTypes();
            Log("[diag]   Types: " + types.Length + " PASS");
        }
        catch (ReflectionTypeLoadException rtle)
        {
            Log("[diag]   RTLE! Types=" + (rtle.Types?.Length ?? 0));
            if (rtle.LoaderExceptions != null)
                foreach (var le in rtle.LoaderExceptions)
                    if (le != null) Log("[diag]     " + le.GetType().Name + ": " + le.Message);
        }
        catch (Exception ex) { Log("[diag]   FAIL: " + ex.GetType().Name + ": " + ex.Message); }

        // Test 3: GetTypes on Meadow.F7 (complex type hierarchy)
        try
        {
            Log("[diag] Test 3: GetTypes on Meadow.F7");
            var asm = typeof(Meadow.Devices.F7FeatherV2).Assembly;
            Log("[diag]   FullName: " + (asm?.FullName ?? "(null)"));
            var types = asm.GetTypes();
            Log("[diag]   Types: " + types.Length + " PASS");
        }
        catch (ReflectionTypeLoadException rtle)
        {
            Log("[diag]   RTLE! Types=" + (rtle.Types?.Length ?? 0));
            if (rtle.LoaderExceptions != null)
                foreach (var le in rtle.LoaderExceptions)
                    if (le != null) Log("[diag]     " + le.GetType().Name + ": " + le.Message);
        }
        catch (Exception ex) { Log("[diag]   FAIL: " + ex.GetType().Name + ": " + ex.Message); }

        // Test 4: LoadFrom App.dll and GetTypes
        try
        {
            Log("[diag] Test 4: App.dll LoadFrom + GetTypes");
            var asm = Assembly.LoadFrom("/meadow0/App.dll");
            Log("[diag]   FullName: " + (asm?.FullName ?? "(null)"));
            if (asm != null)
            {
                var types = asm.GetTypes();
                Log("[diag]   Types: " + types.Length);
                foreach (var t in types)
                    Log("[diag]     " + (t?.FullName ?? "(null)"));
                Log("[diag]   PASS");
            }
        }
        catch (ReflectionTypeLoadException rtle)
        {
            Log("[diag]   RTLE! Types=" + (rtle.Types?.Length ?? 0));
            if (rtle.LoaderExceptions != null)
                foreach (var le in rtle.LoaderExceptions)
                    if (le != null) Log("[diag]     " + le.GetType().Name + ": " + le.Message);
        }
        catch (Exception ex) { Log("[diag]   FAIL: " + ex.GetType().Name + ": " + ex.Message); }

        Log("[diag] === Diagnostics complete, proceeding to blink ===");

        // --- Blink test (known working) ---
        try
        {
            Log("[blinky] Creating F7FeatherV2...");
            var device = new F7FeatherV2();
            Log("[blinky] F7FeatherV2 created OK");

            var ledR = device.CreateDigitalOutputPort(device.Pins.OnboardLedRed, false);
            Log("[blinky] LED port created OK");

            for (int i = 0; i < 3; i++)
            {
                ledR.State = true;
                Log("[blinky] LED ON " + i);
                Thread.Sleep(500);

                ledR.State = false;
                Log("[blinky] LED OFF " + i);
                Thread.Sleep(500);
            }

            Log("[blinky] Blink complete");
        }
        catch (Exception ex)
        {
            Log("[blinky] Exception: " + ex.GetType().FullName);
            Log("[blinky] Message: " + ex.Message);
        }

        return 42;
    }
}
