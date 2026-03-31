# Track 11: TLS/MbedTLS — Handoff

## Status: Phase 1-4 COMPLETE (scaffolding), Phase 5-6 PENDING (build + validate)

## What was done

### Phase 1: Port mono-mbedtls.c
- Ported `mono/mono/mbedtls/mono-mbedtls.c` → `apps/examples/mono/mono_mbedtls.c`
- Stripped legacy Mono deps: `eglib/glib.h` → stdlib, `g_malloc` → malloc, removed MonoFDHandle
- Preserved all mbedTLS logic: init, connect, handshake, read, write, close
- Copied `root-ca-der.h` (10,609 lines, Mozilla root CAs in DER format)
- Added accessor functions for the CryptoNative shim layer

### Phase 2-3: CryptoNative_Ssl* Shim
- Created `apps/examples/mono/pal_ssl_mbedtls.c` (~600 lines)
- Implements the OpenSSL-compatible CryptoNative_* P/Invoke interface using mbedTLS
- **BIO ring buffer**: 32KB ring buffers implement OpenSSL's BIO model
  - InputBio: managed writes received ciphertext, mbedTLS reads from it
  - OutputBio: mbedTLS writes outgoing ciphertext, managed reads from it
- **SSL lifecycle**: SslCtxCreate → SslCreate → SslSetBio → SslDoHandshake → SslRead/Write → SslShutdown → SslDestroy
- Uses shared global config from `mono_mbedtls_init()` (root CAs, entropy, RNG, client certs)
- ~60 stub functions for non-critical features (session caching, ALPN, OCSP, etc.)
- Error code mapping: mbedTLS → PAL_SSL_ERROR_* → managed SecurityStatusPalErrorCode

### Phase 4: P/Invoke Wiring
- Created `apps/examples/mono/mappings-crypto-native.h` — mapping table (~100 entries)
- Updated `mono_main.c` P/Invoke override to route:
  - `System.Security.Cryptography.Native.OpenSsl` → `crypto_native_mappings`
  - `mbedtls` → `mbedtls_mappings` (legacy Meadow interface)
- Added crypto library to no-op fallback for unmapped functions

### Build Integration
- Updated `apps/examples/mono/Makefile`:
  - Added `mono_mbedtls.c` and `pal_ssl_mbedtls.c` to CSRCS
  - Added mbedTLS include path: `-I$(MBEDTLS_DIR)`

## What's left

### Phase 5: Build & Fix Compilation Issues
- Build firmware with TLS support: `./build.sh --force`
- Fix any compilation errors (likely: NuttX header compat, mbedTLS API version differences)
- Verify linker resolves all symbols against libmbedtls.a/libmbedcrypto.a/libmbedx509.a

### Phase 6: Validation
- Test in emulator (needs network passthrough setup for real TLS)
- Test on hardware with WiFi
- Start with simple: does `SslStream` creation + handshake work?
- Then: HTTPS GET via HttpClient

## Key design decisions
1. **Shared global config**: All SSL sessions use the config from `mono_mbedtls_init()` — keeps things simple for single-connection embedded use
2. **BIO ring buffer**: 32KB is enough for standard TLS record sizes (max 16KB + overhead)
3. **No TLS 1.3**: Disabled — mbedTLS 3.x can support it but adds complexity
4. **No session caching/ALPN/OCSP**: Stubbed as no-ops — can be added later if needed
5. **Managed code unchanged**: SslStreamPal.Unix.cs, Interop.OpenSsl.cs, Interop.Ssl.cs all work as-is

## Files created/modified
| File | Action |
|------|--------|
| `apps/examples/mono/mono_mbedtls.c` | NEW — ported mbedTLS provider |
| `apps/examples/mono/root-ca-der.h` | COPIED — Mozilla root CAs |
| `apps/examples/mono/pal_ssl_mbedtls.c` | NEW — CryptoNative shim |
| `apps/examples/mono/mappings-crypto-native.h` | NEW — P/Invoke mapping table |
| `apps/examples/mono/mono_main.c` | MODIFIED — added crypto library routing |
| `apps/examples/mono/Makefile` | MODIFIED — added source files + mbedTLS include |
| `conductor/tracks/mono-upgrade-11-tls/plan.md` | UPDATED — full architecture analysis |
