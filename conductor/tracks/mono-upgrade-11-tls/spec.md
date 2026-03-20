# Track 11: TLS / Networking — Mbed TLS Integration

## Overview
Port the Mbed TLS integration from the Mono 6.9.0 fork to the .NET 10 Mono runtime, enabling HTTPS and secure networking for Meadow applications.

## Background

### Current Mbed TLS Integration (Mono 6.9.0 Fork)
- `mono/mbedtls/mono-mbedtls.c` — 12,424 bytes, full TLS provider
- `mono/mbedtls/root-ca-der.h` — 960KB DER-format root CA certificates
- Meadow client certificate support via `meadow_client_cert.h`
- Entropy polling fix (10ms sleep instead of 1s)
- Linked against Mbed TLS 3.2.1 library (`Meadow/mbedtls/`)

### .NET 10 Mono TLS Architecture
- TLS has been restructured — old Mono used `btls` (BoringSSL) or platform-native TLS
- .NET 10 uses managed `SslStream` backed by platform TLS provider
- Need to understand how to plug Mbed TLS in as the platform provider

## Functional Requirements
1. Understand .NET 10 Mono's TLS provider interface
2. Port `mono-mbedtls.c` to the new TLS provider architecture
3. Port root CA certificates
4. Port Meadow client certificate support
5. Port entropy polling optimization
6. Enable HTTPS connections from managed code

## Acceptance Criteria
- [ ] TLS provider compiles and links with .NET 10 Mono
- [ ] HTTPS connection to a public server succeeds (e.g., `https://httpbin.org/get`)
- [ ] Server certificate validation works (valid certs accepted, invalid rejected)
- [ ] Client certificate authentication works (Meadow device certs)
- [ ] Entropy source works on NuttX (`/dev/random`)
- [ ] No TLS connection delays from entropy polling
- [ ] Validated on emulator (with network passthrough) and hardware

## Out of Scope
- WiFi connectivity (separate from TLS — depends on ESP32/ESPCP)
- HTTP/2 or advanced TLS features
- Certificate management UI
