#if SECURITY_DEP && MONO_FEATURE_MBEDTLS
#if MONO_SECURITY_ALIAS
extern alias MonoSecurity;
#endif

using System.Collections.Generic;

namespace Mono.MbedTls
{
	public enum MbedtlsSslError
	{
		SSL_CRYPTO_IN_PROGRESS = 0x7000,
		SSL_FEATURE_UNAVAILABLE = 0x7080,
		SSL_BAD_INPUT_DATA = 0x7100,
		SSL_INVALID_MAC = 0x7180,
		SSL_INVALID_RECORD = 0x7200,
		SSL_CONN_EOF = 0x7280,
		SSL_DECODE_ERROR = 0x7300,
		SSL_NO_RNG = 0x7400,
		SSL_NO_CLIENT_CERTIFICATE = 0x7480,
		SSL_UNSUPPORTED_EXTENSION = 0x7500,
		SSL_NO_APPLICATION_PROTOCOL = 0x7580,
		SSL_PRIVATE_KEY_REQUIRED = 0x7600,
		SSL_CA_CHAIN_REQUIRED = 0x7680,
		SSL_UNEXPECTED_MESSAGE = 0x7700,
		SSL_FATAL_ALERT_MESSAGE = 0x7780,
		SSL_UNRECOGNIZED_NAME = 0x7800,
		SSL_PEER_CLOSE_NOTIFY = 0x7880,
		SSL_BAD_CERTIFICATE = 0x7A00,
		SSL_ALLOC_FAILED = 0x7F00,
		SSL_HW_ACCEL_FAILED = 0x7F80,
		SSL_HW_ACCEL_FALLTHROUGH = 0x6F80,
		SSL_BAD_PROTOCOL_VERSION = 0x6E80,
		SSL_HANDSHAKE_FAILURE = 0x6E00,
		SSL_SESSION_TICKET_EXPIRED = 0x6D80,
		SSL_PK_TYPE_MISMATCH = 0x6D00,
		SSL_UNKNOWN_IDENTITY = 0x6C80,
		SSL_INTERNAL_ERROR = 0x6C00,
		SSL_COUNTER_WRAPPING = 0x6B80,
		SSL_WAITING_SERVER_HELLO_RENEGO = 0x6B00,
		SSL_HELLO_VERIFY_REQUIRED = 0x6A80,
		SSL_BUFFER_TOO_SMALL = 0x6A00,
		SSL_WANT_READ = 0x6900,
		SSL_WANT_WRITE = 0x6880,
		SSL_TIMEOUT = 0x6800,
		SSL_CLIENT_RECONNECT = 0x6780,
		SSL_UNEXPECTED_RECORD = 0x6700,
		SSL_NON_FATAL = 0x6680,
		SSL_ILLEGAL_PARAMETER = 0x6600,
		SSL_CONTINUE_PROCESSING = 0x6580,
		SSL_ASYNC_IN_PROGRESS = 0x6500,
		SSL_EARLY_MESSAGE = 0x6480,
		SSL_UNEXPECTED_CID = 0x6000,
		SSL_VERSION_MISMATCH = 0x5F00,
		SSL_BAD_CONFIG = 0x5E80,
	}

	public static class MbedtlsSslErrorExtensions
	{
		private static readonly Dictionary<MbedtlsSslError, string> ErrorStrings = new Dictionary<MbedtlsSslError, string>
		{
			{ MbedtlsSslError.SSL_CRYPTO_IN_PROGRESS, "A cryptographic operation is in progress. Try again later." },
			{ MbedtlsSslError.SSL_FEATURE_UNAVAILABLE, "The requested feature is not available." },
			{ MbedtlsSslError.SSL_BAD_INPUT_DATA, "Bad input parameters to function." },
			{ MbedtlsSslError.SSL_INVALID_MAC, "Verification of the message MAC failed." },
			{ MbedtlsSslError.SSL_INVALID_RECORD, "An invalid SSL record was received." },
			{ MbedtlsSslError.SSL_CONN_EOF, "The connection indicated an EOF." },
			{ MbedtlsSslError.SSL_DECODE_ERROR, "A message could not be parsed due to a syntactic error." },
			{ MbedtlsSslError.SSL_NO_RNG, "No RNG was provided to the SSL module." },
			{ MbedtlsSslError.SSL_NO_CLIENT_CERTIFICATE, "No client certification received from the client, but required by the authentication mode." },
			{ MbedtlsSslError.SSL_UNSUPPORTED_EXTENSION, "Client received an extended server hello containing an unsupported extension." },
			{ MbedtlsSslError.SSL_NO_APPLICATION_PROTOCOL, "No ALPN protocols supported that the client advertises." },
			{ MbedtlsSslError.SSL_PRIVATE_KEY_REQUIRED, "The own private key or pre-shared key is not set, but needed." },
			{ MbedtlsSslError.SSL_CA_CHAIN_REQUIRED, "No CA Chain is set, but required to operate." },
			{ MbedtlsSslError.SSL_UNEXPECTED_MESSAGE, "An unexpected message was received from our peer." },
			{ MbedtlsSslError.SSL_FATAL_ALERT_MESSAGE, "A fatal alert message was received from our peer." },
			{ MbedtlsSslError.SSL_UNRECOGNIZED_NAME, "No server could be identified matching the client's SNI." },
			{ MbedtlsSslError.SSL_PEER_CLOSE_NOTIFY, "The peer notified us that the connection is going to be closed." },
			{ MbedtlsSslError.SSL_BAD_CERTIFICATE, "Processing of the Certificate handshake message failed." },
			{ MbedtlsSslError.SSL_ALLOC_FAILED, "Memory allocation failed." },
			{ MbedtlsSslError.SSL_HW_ACCEL_FAILED, "Hardware acceleration function returned with error." },
			{ MbedtlsSslError.SSL_HW_ACCEL_FALLTHROUGH, "Hardware acceleration function skipped / left alone data." },
			{ MbedtlsSslError.SSL_BAD_PROTOCOL_VERSION, "Handshake protocol not within min/max boundaries." },
			{ MbedtlsSslError.SSL_HANDSHAKE_FAILURE, "The handshake negotiation failed." },
			{ MbedtlsSslError.SSL_SESSION_TICKET_EXPIRED, "Session ticket has expired." },
			{ MbedtlsSslError.SSL_PK_TYPE_MISMATCH, "Public key type mismatch (eg, asked for RSA key exchange and presented EC key)." },
			{ MbedtlsSslError.SSL_UNKNOWN_IDENTITY, "Unknown identity received (eg, PSK identity)." },
			{ MbedtlsSslError.SSL_INTERNAL_ERROR, "Internal error (eg, unexpected failure in lower-level module)." },
			{ MbedtlsSslError.SSL_COUNTER_WRAPPING, "A counter would wrap (eg, too many messages exchanged)." },
			{ MbedtlsSslError.SSL_WAITING_SERVER_HELLO_RENEGO, "Unexpected message at ServerHello in renegotiation." },
			{ MbedtlsSslError.SSL_HELLO_VERIFY_REQUIRED, "DTLS client must retry for hello verification." },
			{ MbedtlsSslError.SSL_BUFFER_TOO_SMALL, "A buffer is too small to receive or write a message." },
			{ MbedtlsSslError.SSL_WANT_READ, "No data of requested type currently available on underlying transport." },
			{ MbedtlsSslError.SSL_WANT_WRITE, "Connection requires a write call." },
			{ MbedtlsSslError.SSL_TIMEOUT, "The operation timed out." },
			{ MbedtlsSslError.SSL_CLIENT_RECONNECT, "The client initiated a reconnect from the same port." },
			{ MbedtlsSslError.SSL_UNEXPECTED_RECORD, "Record header looks valid but is not expected." },
			{ MbedtlsSslError.SSL_NON_FATAL, "The alert message received indicates a non-fatal error." },
			{ MbedtlsSslError.SSL_ILLEGAL_PARAMETER, "A field in a message was incorrect or inconsistent with other fields." },
			{ MbedtlsSslError.SSL_CONTINUE_PROCESSING, "Internal-only message signaling that further message-processing should be done." },
			{ MbedtlsSslError.SSL_ASYNC_IN_PROGRESS, "The asynchronous operation is not completed yet." },
			{ MbedtlsSslError.SSL_EARLY_MESSAGE, "Internal-only message signaling that a message arrived early." },
			{ MbedtlsSslError.SSL_UNEXPECTED_CID, "An encrypted DTLS-frame with an unexpected CID was received." },
			{ MbedtlsSslError.SSL_VERSION_MISMATCH, "An operation failed due to an unexpected version or configuration." },
			{ MbedtlsSslError.SSL_BAD_CONFIG, "Invalid value in SSL config." },
		};

		public static string ToErrorString(this MbedtlsSslError error)
		{
			return ErrorStrings.TryGetValue(error, out var value) ? value : $"Unknown error code: {error}";
		}
	}
}
#endif