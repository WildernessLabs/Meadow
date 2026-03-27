// Diagnostic wrapper for MeadowOS — isolates the device initialization failure.
// Tries to create F7FeatherV2 step-by-step to pinpoint the NullReferenceException.

using System;
using System.Reflection;
using System.Runtime.InteropServices;

class DiagWrapper
{
    [DllImport("nuttx")]
    static extern int open(string path, int flags);

    [DllImport("nuttx")]
    static extern int close(int fd);

    [DllImport("nuttx")]
    static extern uint meadow_os_hardware_version();

    static int Main()
    {
        try { Console.WriteLine("[diag] DiagWrapper starting"); } catch { }

        // Step 1: Test /dev/upd directly
        try
        {
            int fd = open("/dev/upd", 2); // O_RDWR
            try { Console.WriteLine("[diag] open(/dev/upd) => " + fd); } catch { }
            if (fd >= 0) close(fd);
        }
        catch (Exception ex)
        {
            try { Console.WriteLine("[diag] open(/dev/upd) threw: " + ex.GetType().Name); } catch { }
        }

        // Step 2: Test meadow_os_hardware_version P/Invoke
        try
        {
            uint hwver = meadow_os_hardware_version();
            try { Console.WriteLine("[diag] meadow_os_hardware_version => " + hwver); } catch { }
        }
        catch (Exception ex)
        {
            try { Console.WriteLine("[diag] hw_version threw: " + ex.GetType().Name + ": " + ex.Message); } catch { }
        }

        // Step 3: Load App.dll and test type resolution
        try
        {
            var appAsm = Assembly.LoadFrom("/meadow0/App.dll");
            try { Console.WriteLine("[diag] Loaded App.dll OK"); } catch { }

            // Try loading MeadowApp specifically
            try
            {
                var meadowAppType = appAsm.GetType("MeadowApp", false);
                if (meadowAppType != null)
                {
                    try { Console.WriteLine("[diag] MeadowApp type found: " + meadowAppType.FullName); } catch { }
                    try { Console.WriteLine("[diag] MeadowApp base: " + (meadowAppType.BaseType?.FullName ?? "null")); } catch { }
                    try { Console.WriteLine("[diag] MeadowApp base.base: " + (meadowAppType.BaseType?.BaseType?.FullName ?? "null")); } catch { }
                }
                else
                {
                    try { Console.WriteLine("[diag] MeadowApp type not found"); } catch { }
                }
            }
            catch (Exception te)
            {
                try { Console.WriteLine("[diag] GetType(MeadowApp) failed: " + te.GetType().Name + ": " + te.Message); } catch { }
                if (te.InnerException != null)
                    try { Console.WriteLine("[diag]   inner: " + te.InnerException.GetType().Name + ": " + te.InnerException.Message); } catch { }
            }

            // Try loading individual assemblies from the chain
            string[] depsToCheck = { "Meadow.Contracts.dll", "Meadow.F7.dll", "Meadow.Logging.dll", "Meadow.Units.dll" };
            foreach (var dep in depsToCheck)
            {
                try
                {
                    var depAsm = Assembly.LoadFrom("/meadow0/" + dep);
                    try { Console.WriteLine("[diag] Loaded " + dep + " => " + depAsm.GetName().Name); } catch { }
                }
                catch (Exception de)
                {
                    try { Console.WriteLine("[diag] Failed to load " + dep + ": " + de.GetType().Name); } catch { }
                }
            }

            // Try GetTypes with module-level enumeration
            try
            {
                foreach (var module in appAsm.GetModules())
                {
                    try { Console.WriteLine("[diag] Module: " + module.Name); } catch { }
                    try
                    {
                        var mtypes = module.GetTypes();
                        try { Console.WriteLine("[diag] Module types: " + mtypes.Length); } catch { }
                        foreach (var mt in mtypes)
                            try { Console.WriteLine("[diag]   type: " + mt.FullName); } catch { }
                    }
                    catch (Exception me)
                    {
                        try { Console.WriteLine("[diag] Module GetTypes: " + me.GetType().Name + ": " + me.Message); } catch { }
                    }
                }
            }
            catch (Exception me2)
            {
                try { Console.WriteLine("[diag] GetModules: " + me2.GetType().Name); } catch { }
            }
        }
        catch (Exception ex)
        {
            try { Console.WriteLine("[diag] App.dll load failed: " + ex.GetType().Name + ": " + ex.Message); } catch { }
        }

        // Step 4: Load MeadowCore.dll and try MeadowOS.Main
        try
        {
            var asm = Assembly.LoadFrom("/meadow0/MeadowCore.dll");
            try { Console.WriteLine("[diag] Loaded MeadowCore.dll OK"); } catch { }

            var meadowOsType = asm.GetType("Meadow.MeadowOS", true);
            try { Console.WriteLine("[diag] Got MeadowOS type"); } catch { }

            var mainMethod = meadowOsType.GetMethod("Main", BindingFlags.Public | BindingFlags.Static);
            if (mainMethod == null)
            {
                try { Console.WriteLine("[diag] ERROR: MeadowOS.Main not found"); } catch { }
                return 2;
            }
            try { Console.WriteLine("[diag] Invoking MeadowOS.Main with --root /meadow0..."); } catch { }

            // Pass --root so MeadowOS searches for App.dll at /meadow0/
            // instead of using GetEntryAssembly() (which returns DiagWrapper)
            var task = mainMethod.Invoke(null, new object[] { new string[] { "--root", "/meadow0" } });
            if (task is System.Threading.Tasks.Task t)
            {
                t.Wait();
            }

            try { Console.WriteLine("[diag] MeadowOS.Main completed"); } catch { }
        }
        catch (TargetInvocationException tie)
        {
            var inner = tie.InnerException ?? tie;
            LogException("[diag] TargetInvocationException", inner);
        }
        catch (Exception ex)
        {
            LogException("[diag] Exception", ex);
        }

        return 42;
    }

    static void LogException(string label, Exception ex)
    {
        try
        {
            Console.WriteLine(label + ": " + ex.GetType().FullName);
            Console.WriteLine("[diag] Message: " + ex.Message);
            try
            {
                var st = ex.StackTrace;
                if (st != null)
                    Console.WriteLine("[diag] StackTrace: " + st);
                else
                    Console.WriteLine("[diag] StackTrace: (null)");
            }
            catch (Exception stEx)
            {
                Console.WriteLine("[diag] StackTrace failed: " + stEx.GetType().Name);
            }

            if (ex.InnerException != null)
            {
                LogException("[diag] InnerException", ex.InnerException);
            }
        }
        catch { }
    }
}
