using System;
using System.Runtime.InteropServices;
using System.Text;

class Program
{
    [DllImport("libSystem.Native", EntryPoint = "SystemNative_SyslogWrite")]
    static extern void SyslogWrite(byte[] buffer, int length);

    static void Log(string message)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(message + "\n");
        SyslogWrite(bytes, bytes.Length);
    }

    static void Main()
    {
        Log("=== Mono Mini Test Runner (standalone) ===");

        // Redirect Console.Out so TestDriver output goes to syslog
        Console.SetOut(new SyslogTextWriter());

        var suites = new (string Name, Type Type)[]
        {
            ("basic",       typeof(BasicTests)),
            ("arrays",      typeof(ArrayTests)),
            ("basic-calls", typeof(CallsTests)),
            ("basic-float", typeof(FloatTests)),
            ("basic-long",  typeof(LongTests)),
            ("basic-math",  typeof(MathTests)),
            ("objects",     typeof(ObjectTests.Tests)),
            ("exceptions",  typeof(ExceptionTests)),
            ("generics",    typeof(GenericsTests)),
            ("gshared",     typeof(GSharedTests)),
            // ("gc-test", typeof(GcTests)),
        };

        int totalRan = 0;
        int totalFailed = 0;
        int totalSkipped = 0;

        foreach (var suite in suites)
        {
            Log($"=== Running: {suite.Name} ===");
            try
            {
                var reporter = new TestDriverReporter();
                // -v for verbose output; try/catch in TestDriver handles per-test crashes
                int failed = TestDriver.RunTests(suite.Type, new[] { "-v" }, reporter);
                totalRan += reporter.ExecutedTests;
                totalFailed += reporter.FailedTests;
                totalSkipped += reporter.SkippedTests;
                Log($"--- {suite.Name}: {reporter.ExecutedTests} ran, {reporter.SkippedTests} skipped, {reporter.FailedTests} failed ---");
            }
            catch (Exception ex)
            {
                Log($"!!! {suite.Name} CRASHED: {ex.GetType().Name}: {ex.Message}");
                for (var inner = ex.InnerException; inner != null; inner = inner.InnerException)
                    Log($"    Inner: {inner.GetType().Name}: {inner.Message}");
                totalFailed++;
            }
        }

        Log($"=== TOTAL: {totalRan} ran, {totalSkipped} skipped, {totalFailed} failed ===");
        Log("=== Test Runner Complete ===");
    }
}

class SyslogTextWriter : System.IO.TextWriter
{
    [DllImport("libSystem.Native", EntryPoint = "SystemNative_SyslogWrite")]
    static extern void SyslogWrite(byte[] buffer, int length);

    public override Encoding Encoding => Encoding.UTF8;

    public override void WriteLine(string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes((value ?? "") + "\n");
        SyslogWrite(bytes, bytes.Length);
    }

    public override void Write(string value)
    {
        if (!string.IsNullOrEmpty(value))
        {
            byte[] bytes = Encoding.UTF8.GetBytes(value);
            SyslogWrite(bytes, bytes.Length);
        }
    }
}
