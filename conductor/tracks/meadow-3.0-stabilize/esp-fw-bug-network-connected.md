# Known Issue: ESP coprocessor never fires `NetworkConnectedEvent` when auto-connected before mono

## Symptom

`Meadow.Cloud` connection service starts but stays in `Disconnected` state
indefinitely (`waiting for network connection (N s)` log spam), even
though:

- `wlan0` has an IP and gateway (`ifconfig` shows them).
- DNS resolves (`Dns.GetHostAddresses("example.com")` returns results).
- HTTPS GET against arbitrary hosts succeeds.

`Esp32WiFiAdapter.CurrentState` is stuck at `Connecting`, so
`IsConnected` returns false, and the cloud state machine never
authenticates.

## Root cause

Trace logs show the ESP fires `NetworkConnectingEvent` (function id 47,
twice — once for the boot auto-connect, once for the managed-code
`ConnectToDefaultAccessPoint` call) but **never** fires
`NetworkConnectedEvent` (function id 36) or `NetworkGotIpEvent` (48).

In the ESP firmware
(`Meadow-ESP32/components/F7Services/WiFi/WiFiRequestHandler.cpp:1521`)
the `ConnectToAccessPoint` path looks like:

```cpp
StatusCodes::StatusCodes WiFiRequestHandler::ConnectToAccessPoint(...)
{
    RaiseNetworkConnectingEvent();              // ← fires (47)
    ...
    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (bits & WIFI_HAVE_IP_BIT)
    {
        return (StatusCodes::WiFiAlreadyStarted);   // ← early return, no Connected event
    }
    ...
    // normal path → eventually fires NetworkConnectedEvent via
    // StationConnectedEvent → RaiseConnectToAccessPointEvent
}
```

Because the ESP auto-connects to the saved access point during its own
boot (long before mono is enabled), `WIFI_HAVE_IP_BIT` is already set by
the time the managed code's `_ = wifiAdapter.ConnectToDefaultAccessPoint(...)`
fire-and-forget call reaches the ESP. The firmware hits the early return
and the managed adapter is stuck at `Connecting` forever.

Additionally, the events the ESP *did* fire during its boot
auto-connect get dropped on the NuttX side: in
`espcp_pass_to_managed_event_handler` (NuttX
`espcp_event_handlers.c:642`) any event raised while the
`HCOM_BBREG_USER_RQST_MONO_ENABLE_BIT` is clear is logged and discarded,
so by the time mono starts, the original `NetworkConnectedEvent` is
already gone.

## Two observed flavors

**Flavor A — fresh-boot race (most common).** ESP auto-connects before
mono enables; lwip has an IP (DNS, HTTP, HTTPS all work natively); but
managed `Esp32WiFiAdapter.CurrentState` is stuck at `Connecting`.
Anything depending on `IWiFiNetworkAdapter.IsConnected` (Meadow.Cloud,
user code) stays "disconnected" forever.

**Flavor B — redeploy desync.** Run app → cold-reset → run = works.
Run app → `meadow app run` again **without reset** = network completely
dead: `wlan0` status `Down`, IP `0.0.0.0`, every DNS / HTTP call fails
with `Try again`. During the heavy HCOM file transfer of a redeploy, the
ESP appears to fire a `NetworkDisconnectedEvent` (NuttX side clears
`wlan0` IP) which gets dropped because mono is disabled. ESP later
re-associates and sets `WIFI_HAVE_IP_BIT`, but mono is still disabled so
the `NetworkConnectedEvent` is also dropped. When mono re-enables and
asks `ConnectToDefaultAccessPoint`, the firmware hits the same
WiFiAlreadyStarted early-return — but this time lwip *also* has no IP.
Result: both sides desynced, no traffic at all.

## Managed-side workaround (shipped in 3.0)

`Meadow.Core/source/implementations/f7/Meadow.F7/Devices/Esp32Coprocessor/Esp32WiFiAdapter.cs`
runs a two-phase watchdog whenever the adapter enters `Connecting`:

- **Phase 1 (Flavor A recovery, ~10 s):** poll `lwip` via
  `NetworkInterface.GetAllNetworkInterfaces()`. If any non-loopback
  interface has an IPv4 address, force `CurrentState = Connected` and
  raise `NetworkConnected`. (Bypasses `LoadAdapterInfo`'s
  `NetworkInterfaceType.Wireless80211` filter because the NuttX-side
  `NetworkInterface` impl reports `wlan0` as type `Unknown`.)
- **Phase 2 (Flavor B recovery):** if 10 s pass with no IP, send
  `DisconnectFromAccessPoint` to the ESP, wait 1 s, then re-issue
  `ConnectToDefaultAccessPoint`. This forces the firmware out of its
  "already connected, no event" state — clearing `WIFI_HAVE_IP_BIT` and
  triggering the normal `RaiseConnectToAccessPointEvent` path on the
  next associate. Disruptive (a few seconds of WiFi blip) but
  recoverable in the field.

Phase 2 is gated by Phase 1 failing, so the happy path costs nothing
beyond a few `GetAllNetworkInterfaces` polls. The watchdog only runs
while `CurrentState == Connecting`, so it doesn't fire spuriously on
later state changes.

## Proper fix (deferred — needs ESP coprocessor rebuild)

In `WiFiRequestHandler::ConnectToAccessPoint`, when `WIFI_HAVE_IP_BIT`
is already set on entry, fire `RaiseConnectToAccessPointEvent(CompletedOk)`
before returning `WiFiAlreadyStarted`. Approximate diff:

```cpp
if (bits & WIFI_HAVE_IP_BIT)
{
    // ESP was already associated (boot auto-connect). Let managed code
    // know — without this the C# state machine never sees Connected.
    RaiseConnectToAccessPointEvent(StatusCodes::CompletedOk);
    return (StatusCodes::WiFiAlreadyStarted);
}
```

This requires:
1. Rebuild `meadow_comms.bin` from the `Meadow-ESP32` repo.
2. Flash the new coprocessor firmware
   (`meadow firmware write Coprocessor`).
3. Bump the coprocessor protocol/firmware version string so devices in
   the field don't silently mismatch.

Until then, the managed workaround is sufficient and safe — it only
forces a state transition when lwip can actually confirm we have an IP.

## Optional secondary fix (NuttX)

`espcp_pass_to_managed_event_handler` could buffer the last-known
network state (one `NetworkConnectedEvent` + one `NetworkGotIpEvent`)
and replay them to managed code when mono enables. This would also fix
the boot race without an ESP firmware change, but adds NuttX-side state.
Not pursued for 3.0.
