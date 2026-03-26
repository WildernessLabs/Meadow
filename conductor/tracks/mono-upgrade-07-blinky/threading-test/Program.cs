using System;
using System.Threading;
using System.Threading.Tasks;

class Program
{
    static int Main()
    {
        Console.WriteLine("Hello from Meadow!");

        // Test 1: Thread.Sleep
        Console.WriteLine("Thread.Sleep(100)...");
        Thread.Sleep(100);
        Console.WriteLine("Thread.Sleep OK");

        // Test 2: Task.Delay
        Console.WriteLine("Task.Delay(500)...");
        Task.Delay(500).Wait();
        Console.WriteLine("Task.Delay OK");

        // Test 3: ThreadPool.QueueUserWorkItem
        Console.WriteLine("ThreadPool test...");
        var mre = new ManualResetEventSlim();
        ThreadPool.QueueUserWorkItem(_ => {
            Console.WriteLine("ThreadPool callback running");
            mre.Set();
        });
        if (!mre.Wait(5000))
            return 1;
        Console.WriteLine("ThreadPool OK");

        // Test 4: Manual thread creation
        Console.WriteLine("Thread creation test...");
        var done = new ManualResetEventSlim();
        var t = new Thread(() => {
            Console.WriteLine("New thread running");
            done.Set();
        });
        t.Start();
        if (!done.Wait(5000))
            return 2;
        Console.WriteLine("Thread creation OK");

        Console.WriteLine("All tests passed!");
        return 42;
    }
}
