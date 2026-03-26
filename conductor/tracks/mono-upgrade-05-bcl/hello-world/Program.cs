class Program
{
    static int Main()
    {
        try
        {
            System.Console.WriteLine("Hello from Meadow!");
        }
        catch (System.Exception)
        {
            // Console.Write may throw on NuttX CDCACM (EBADF when no USB host)
            // The message was already written to syslog via sysn_write
        }
        return 42;
    }
}
