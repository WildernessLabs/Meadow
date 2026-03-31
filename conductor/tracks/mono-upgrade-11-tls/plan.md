# Implementation Plan: TLS / Networking (MbedTLS on .NET 10)

## Architecture Analysis

### How .NET 10 SslStream works (compile-time platform selection)
```
SslStream.AuthenticateAsClient()
  → SslStreamPal.Unix.cs (compile-time selected)
    → Interop.OpenSsl.AllocateSslHandle()
      → Interop.Ssl.SslCtxCreate()  [P/Invoke → CryptoNative_SslCtxCreate]
      → Interop.Ssl.SslCreate()     [P/Invoke → CryptoNative_SslCreate]
    → Interop.OpenSsl.DoSslHandshake()
      → Interop.Ssl.SslDoHandshake() [P/Invoke → CryptoNative_SslDoHandshake]
    → Interop.OpenSsl.Encrypt/Decrypt()
      → Interop.Ssl.SslWrite/SslRead [P/Invoke → CryptoNative_SslWrite/Read]
```

- **No runtime plugin mechanism** — SslStreamPal is a sealed static class, chosen at compile time
- Unix PAL uses OpenSSL via ~60 `CryptoNative_*` P/Invoke functions
- Transport uses BIO pairs (memory I/O), not raw socket FDs
- SafeSslHandle / SafeSslContextHandle are IntPtr wrappers (don't care what the pointer is)

### Legacy Mono mbedTLS (what we're reusing)
```
MbedTlsContext.cs [DllImport("mbedtls")]
  → mono_mbedtls_init()      — one-time setup: config, entropy, root CAs, client certs
  → mono_mbedtls_connect()   — allocate ssl context, set hostname, attach socket FD
  → mono_mbedtls_handshake() — perform TLS handshake
  → mono_mbedtls_read/write()— encrypted I/O via fixed 4KB buffers
  → mono_mbedtls_close()     — close_notify + free
```

### Integration Strategy

**Approach: Implement `CryptoNative_Ssl*` functions backed by mbedTLS, wired via P/Invoke override.**

The managed SslStreamPal.Unix.cs + Interop.OpenSsl.cs code stays unchanged. Our native
`CryptoNative_*` implementations map OpenSSL concepts to mbedTLS:

| OpenSSL concept | mbedTLS equivalent |
|---|---|
| `SSL_CTX*` | `mbedtls_ssl_config*` (+ entropy, ctr_drbg, cacert) |
| `SSL*` | `mbedtls_ssl_context*` |
| `BIO*` pair | `mbedtls_ssl_set_bio()` with custom send/recv callbacks |
| `SSL_do_handshake` | `mbedtls_ssl_handshake()` |
| `SSL_read/write` | `mbedtls_ssl_read/write()` |
| `X509*` | `mbedtls_x509_crt*` |

Key insight: The managed Interop.OpenSsl layer uses BIO pairs for memory-based I/O
(not raw sockets). It writes plaintext into a BIO, OpenSSL encrypts it, then managed
code reads the ciphertext from another BIO and sends it over the socket. For mbedTLS,
we implement the BIO read/write callbacks to pull/push from managed-provided buffers.

---

## Phase 1: Port mono-mbedtls.c to New Build ✦ NATIVE FOUNDATION
- [x] Research complete — architecture documented above
- [ ] Copy `mono/mono/mbedtls/mono-mbedtls.c` → `apps/examples/mono/mono_mbedtls.c`
- [ ] Copy `mono/mono/mbedtls/root-ca-der.h` → `apps/examples/mono/root-ca-der.h`
- [ ] Remove legacy Mono dependencies: replace `eglib/glib.h` → stdlib, `g_malloc` → malloc
- [ ] Remove `MonoFDHandle`/`SocketHandle` references (not in .NET 10 Mono)
- [ ] Keep mbedTLS API calls unchanged (init, config, ssl_context, handshake, read/write)
- [ ] Keep root CA loading, entropy source, client cert support
- [ ] Verify it compiles standalone with arm-none-eabi-gcc + mbedtls headers

## Phase 2: CryptoNative_Ssl* Shim — Minimal Viable Set
- [ ] Create `apps/examples/mono/pal_ssl_mbedtls.c`
- [ ] Implement wrapper structs:
  - `MbedSslCtx` (wraps mbedtls_ssl_config + entropy + ctr_drbg + cacert) → returned as `SSL_CTX*`
  - `MbedSsl` (wraps mbedtls_ssl_context + net_context) → returned as `SSL*`
  - `MbedBio` (ring buffer for memory I/O) → returned as `BIO*`
- [ ] Implement core functions (minimum for SslStream client):
  1. `CryptoNative_EnsureLibSslInitialized` — no-op or one-time mbedTLS init
  2. `CryptoNative_SslV2_3Method` — return dummy
  3. `CryptoNative_SslCtxCreate` — allocate MbedSslCtx, configure defaults
  4. `CryptoNative_SslCtxSetProtocolOptions` — set min/max TLS version
  5. `CryptoNative_SslCreate` — allocate MbedSsl, setup from config
  6. `CryptoNative_SslSetConnectState` / `SslSetAcceptState`
  7. `CryptoNative_SslSetBio` — link BIO pair for memory I/O
  8. `CryptoNative_SslDoHandshake` — pump handshake via BIO
  9. `CryptoNative_SslRead` / `SslWrite` — encrypted I/O via BIO
  10. `CryptoNative_SslShutdown` — close_notify
  11. `CryptoNative_SslDestroy` / `SslCtxDestroy` — free resources
  12. `CryptoNative_SslSetTlsExtHostName` — set SNI hostname
  13. `CryptoNative_SslGetError` — map mbedTLS errors to PAL_SSL_ERROR_*
  14. `CryptoNative_IsSslStateOK` — check handshake complete
  15. `CryptoNative_SslGetVersion` — return protocol string
  16. `CryptoNative_SslGetPeerCertificate` — return peer cert (for validation)
- [ ] Implement BIO memory I/O (critical):
  - `CryptoNative_CreateMemoryBio` — ring buffer allocation
  - `CryptoNative_BioWrite` — managed writes ciphertext into BIO
  - `CryptoNative_BioRead` — managed reads ciphertext from BIO
  - mbedTLS send/recv callbacks read/write from these buffers
- [ ] Stub remaining CryptoNative_Ssl* functions as no-ops:
  - Session caching, ALPN, OCSP, renegotiation, keylog, etc.
  - Return success / zero to indicate "not supported but don't fail"

## Phase 3: BIO Abstraction Layer (Key Engineering Challenge)
- [ ] Understand the managed Interop.OpenSsl BIO usage pattern:
  - Managed writes plaintext → BIO_write → OpenSSL encrypts → BIO_read ciphertext → send to socket
  - Receive from socket → BIO_write ciphertext → OpenSSL decrypts → BIO_read plaintext → managed reads
- [ ] Implement bidirectional ring buffers that serve as BIO pairs
- [ ] Wire mbedTLS send/recv callbacks to read/write from these buffers
- [ ] Handle WANT_READ/WANT_WRITE correctly (buffer empty/full)

## Phase 4: P/Invoke Override Registration
- [ ] Add CryptoNative_* function entries to `meadow_pinvoke_override` table in `mono_main.c`
- [ ] Library name: `"System.Security.Cryptography.Native.OpenSsl"` (what managed code requests)
- [ ] Also register under `"libSystem.Security.Cryptography.Native.OpenSsl"` (alternate name)
- [ ] Add BIO functions to override table
- [ ] Add minimal CryptoNative_X509* stubs for cert handling
- [ ] Add minimal CryptoNative_Crypto* stubs (error queue, etc.)

## Phase 5: Build Integration
- [ ] Add `mono_mbedtls.c` and `pal_ssl_mbedtls.c` to `apps/examples/mono/Makefile`
- [ ] Link mbedTLS libraries (already in build: `libmbedtls.a`, `libmbedcrypto.a`, `libmbedx509.a`)
- [ ] Add mbedTLS include path to mono app compilation flags
- [ ] Verify firmware builds with TLS support

## Phase 6: Validation
- [ ] Test TLS handshake in emulator (need network passthrough or mock)
- [ ] Test HTTPS GET to public endpoint on hardware
- [ ] Test certificate validation (valid + invalid)
- [ ] Test client certificate authentication
- [ ] Verify HttpClient works from managed Meadow.Core code
- [ ] Memory usage profiling (mbedTLS buffers + TLS overhead)

---

## Key Files

| File | Purpose |
|------|---------|
| `mono/mono/mbedtls/mono-mbedtls.c` | Legacy source — our base |
| `mono/mono/mbedtls/root-ca-der.h` | Root CA certs (DER, Mozilla) |
| `apps/examples/mono/mono_mbedtls.c` | NEW — ported native mbedTLS provider |
| `apps/examples/mono/pal_ssl_mbedtls.c` | NEW — CryptoNative_* shim |
| `apps/examples/mono/mono_main.c` | P/Invoke override table (add entries) |
| `runtime/.../Interop.OpenSsl.cs` | Managed orchestrator (unchanged) |
| `runtime/.../SslStreamPal.Unix.cs` | Platform PAL (unchanged) |
| `runtime/.../Interop.Ssl.cs` | P/Invoke declarations (unchanged) |

## Risks & Mitigations
1. **BIO abstraction mismatch**: OpenSSL BIO is memory-based, mbedTLS uses direct callbacks. Mitigation: ring buffer that bridges the two models.
2. **X509 type incompatibility**: Managed code expects OpenSSL X509 pointers for cert inspection. Mitigation: wrap mbedtls_x509_crt in a struct that the managed code passes around opaquely.
3. **Large CryptoNative surface area**: ~200 functions across SSL, X509, hash, etc. Mitigation: implement only SSL path first, stub everything else.
4. **Memory pressure**: TLS adds ~50-100KB heap overhead per connection. Mitigation: single-connection use case on embedded, already tight on SDRAM.
