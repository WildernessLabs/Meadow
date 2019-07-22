# NuttX with Meadow Tech Debt

1) A delay is needed before mono_main exits to give nuttx time to finish initializing the USB serial connection. This is currently hard coded as 300 milliseconds. A more deterministic method should be found.
