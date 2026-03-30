using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

class JitDiag
{
    [DllImport("libSystem.Native", EntryPoint = "SystemNative_SyslogWrite")]
    static extern void SyslogWrite(byte[] buffer, int length);

    static void Log(string msg)
    {
        byte[] bytes = new byte[msg.Length + 1];
        for (int i = 0; i < msg.Length; i++)
            bytes[i] = (byte)msg[i];
        bytes[msg.Length] = (byte)'\n';
        SyslogWrite(bytes, bytes.Length);
    }

    // Exactly replicates Dictionary.GetBucket IL:
    //   conv.u8, conv.i8, rem, conv.ovf.i, ldelema
    [MethodImpl(MethodImplOptions.NoInlining)]
    static ref int GetBucketLike(int[] buckets, uint hashCode)
    {
        // This should generate the same IL as Dictionary.GetBucket
        long a = (long)(ulong)hashCode;   // conv.u8 (zero-extend)
        long b = (long)(int)buckets.Length; // conv.i8 (sign-extend)
        long rem = a % b;                  // signed 64-bit rem
        int idx = checked((int)rem);       // conv.ovf.i
        return ref buckets[idx];
    }

    // Same but with diagnostics at each step
    [MethodImpl(MethodImplOptions.NoInlining)]
    static int GetBucketDiag(int[] buckets, uint hashCode)
    {
        long a = (long)(ulong)hashCode;
        long b = (long)(int)buckets.Length;
        long rem = a % b;
        // Report the intermediate values
        Log("  a=" + a + " b=" + b + " rem=" + rem + " hi=" + (rem >> 32));
        int idx = checked((int)rem);
        return buckets[idx];
    }

    // Test just the 64-bit remainder without array access
    [MethodImpl(MethodImplOptions.NoInlining)]
    static long LongRem(long a, long b)
    {
        return a % b;
    }

    // Test conv.ovf.i separately
    [MethodImpl(MethodImplOptions.NoInlining)]
    static int CheckedNarrow(long val)
    {
        return checked((int)val);
    }

    static void Pad()
    {
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX01");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX02");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX03");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX04");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX05");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX06");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX07");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX08");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX09");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX10");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX11");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX12");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX13");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX14");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX15");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX16");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX17");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX18");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX19");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX20");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX21");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX22");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX23");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX24");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX25");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX26");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX27");
        Log("PAD_v16_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX28");
    }

    static void Main()
    {
        Log("=== JIT DIAG v16: 64-bit rem + conv.ovf.i ===");

        // TEST 1: 64-bit signed remainder
        Log("--- TEST 1: long remainder ---");
        for (long a = 0; a <= 10; a++)
        {
            try
            {
                long r = LongRem(a, 3);
                Log("LongRem(" + a + ",3)=" + r);
            }
            catch (Exception ex)
            {
                Log("LongRem(" + a + ",3) FAIL: " + ex.GetType().Name);
            }
        }

        // TEST 2: checked((int)longVal)
        Log("--- TEST 2: checked narrowing ---");
        long[] testVals = { 0, 1, 2, 3, -1, -2, 0x100000000L, -0x100000000L };
        foreach (long v in testVals)
        {
            try
            {
                int n = CheckedNarrow(v);
                Log("CheckedNarrow(" + v + ")=" + n);
            }
            catch (Exception ex)
            {
                Log("CheckedNarrow(" + v + ") FAIL: " + ex.GetType().Name);
            }
        }

        // TEST 3: GetBucketDiag (same as Dictionary but with logging)
        Log("--- TEST 3: GetBucketDiag ---");
        int[] buckets = new int[3];
        buckets[0] = 100; buckets[1] = 200; buckets[2] = 300;
        for (uint h = 0; h <= 10; h++)
        {
            try
            {
                int val = GetBucketDiag(buckets, h);
                Log("GetBucketDiag(" + h + ")=" + val);
            }
            catch (Exception ex)
            {
                Log("GetBucketDiag(" + h + ") FAIL: " + ex.GetType().Name);
            }
        }

        // TEST 4: GetBucketLike (same IL as Dictionary.GetBucket)
        Log("--- TEST 4: GetBucketLike ---");
        int[] bk = new int[3];
        bk[0] = 10; bk[1] = 20; bk[2] = 30;
        for (uint h = 0; h <= 10; h++)
        {
            try
            {
                ref int val = ref GetBucketLike(bk, h);
                Log("GetBucketLike(" + h + ")=" + val);
            }
            catch (Exception ex)
            {
                Log("GetBucketLike(" + h + ") FAIL: " + ex.GetType().Name);
            }
        }

        // TEST 5: Dictionary comparison
        Log("--- TEST 5: Dictionary<int,int> ---");
        for (int h = 0; h <= 5; h++)
        {
            try
            {
                var d = new System.Collections.Generic.Dictionary<int, int>();
                d.Add(h, 42);
                Log("Dict.Add(" + h + ") OK");
            }
            catch (Exception ex)
            {
                Log("Dict.Add(" + h + ") FAIL: " + ex.GetType().Name);
            }
        }

        Log("=== JIT DIAG v16: complete ===");
    }
}
