void X_sync_synchronize()
{
          __asm__ __volatile__ (
          "dmb ish"
         );
	__sync_synchronize();
}
