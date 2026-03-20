# Implementation Plan: TLS / Networking

## Phase 1: Research .NET 10 TLS Architecture
- [ ] Study how .NET 10 Mono handles TLS (managed SslStream → native provider)
- [ ] Identify the TLS provider interface/hooks that need implementing
- [ ] Compare with old Mono's btls approach
- [ ] Determine if Mbed TLS can slot in as a platform TLS provider

## Phase 2: Port Mbed TLS Provider
- [ ] Port `mono-mbedtls.c` to new TLS provider interface
- [ ] Update API signatures for .NET 10 mono compatibility
- [ ] Port DER root CA certificates (`root-ca-der.h`)
- [ ] Port entropy polling fix (10ms sleep)
- [ ] Port Meadow client certificate hooks

## Phase 3: Build and Link
- [ ] Add Mbed TLS library to CMake build
- [ ] Link `libmbedtls.a`, `libmbedcrypto.a`, `libmbedx509.a`
- [ ] Resolve any symbol conflicts or missing functions
- [ ] Rebuild firmware with TLS support

## Phase 4: Validation
- [ ] Test HTTPS GET to a public endpoint
- [ ] Test certificate validation (valid + invalid certs)
- [ ] Test client certificate authentication
- [ ] Verify no entropy polling delays
- [ ] Test on emulator (may need network passthrough setup)
- [ ] Test on hardware (via WiFi/ESP32)

## Phase 5: Integration
- [ ] Verify Meadow.Core networking APIs work with new TLS
- [ ] Test `HttpClient` from managed code
- [ ] Document any API changes needed in managed networking stack
