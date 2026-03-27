// Stub entry point for trimming publish project.
// The firmware entry point is MeadowOS.Main in Meadow.dll.
// This file exists only so the trimmer has a root to start from.
class Program
{
    static void Main() => Meadow.MeadowOS.Main(System.Array.Empty<string>()).Wait();
}
