/* Trivial native module for dlopen smoke test.
 *
 * Builds to a position-independent ET_DYN ARM Thumb2 ELF that the on-device
 * modlib loader pulls in via dlopen("/meadow0/dl_smoke.so", RTLD_NOW).
 * No libc dependencies — every symbol must resolve inside this file. */

int meadow_dl_smoke(int x)
{
    return x * 2 + 1;
}
