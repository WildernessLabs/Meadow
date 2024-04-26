#if SECURITY_DEP && MONO_FEATURE_MBEDTLS
#if MONO_SECURITY_ALIAS
extern alias MonoSecurity;
#endif

using System.IO;
using System.Collections.Generic;

namespace Mono.MbedTls
{

	public class MbedTlsIOException : IOException
	{
		public MbedTlsIOException(int hresult, string message) : base(message)
		{
			HResult = hresult;
		}
	}

	public enum MbedTlsSslError
	{
		SSL_CRYPTO_IN_PROGRESS = -0x7000,
		SSL_FEATURE_UNAVAILABLE = -0x7080,
		SSL_BAD_INPUT_DATA = -0x7100,
		SSL_INVALID_MAC = -0x7180,
		SSL_INVALID_RECORD = -0x7200,
		SSL_CONN_EOF = -0x7280,
		SSL_DECODE_ERROR = -0x7300,
		SSL_NO_RNG = -0x7400,
		SSL_NO_CLIENT_CERTIFICATE = -0x7480,
		SSL_UNSUPPORTED_EXTENSION = -0x7500,
		SSL_NO_APPLICATION_PROTOCOL = -0x7580,
		SSL_PRIVATE_KEY_REQUIRED = -0x7600,
		SSL_CA_CHAIN_REQUIRED = -0x7680,
		SSL_UNEXPECTED_MESSAGE = -0x7700,
		SSL_FATAL_ALERT_MESSAGE = -0x7780,
		SSL_UNRECOGNIZED_NAME = -0x7800,
		SSL_PEER_CLOSE_NOTIFY = -0x7880,
		SSL_BAD_CERTIFICATE = -0x7A00,
		SSL_ALLOC_FAILED = -0x7F00,
		SSL_HW_ACCEL_FAILED = -0x7F80,
		SSL_HW_ACCEL_FALLTHROUGH = -0x6F80,
		SSL_BAD_PROTOCOL_VERSION = -0x6E80,
		SSL_HANDSHAKE_FAILURE = -0x6E00,
		SSL_SESSION_TICKET_EXPIRED = -0x6D80,
		SSL_PK_TYPE_MISMATCH = -0x6D00,
		SSL_UNKNOWN_IDENTITY = -0x6C80,
		SSL_INTERNAL_ERROR = -0x6C00,
		SSL_COUNTER_WRAPPING = -0x6B80,
		SSL_WAITING_SERVER_HELLO_RENEGO = -0x6B00,
		SSL_HELLO_VERIFY_REQUIRED = -0x6A80,
		SSL_BUFFER_TOO_SMALL = -0x6A00,
		SSL_WANT_READ = -0x6900,
		SSL_WANT_WRITE = -0x6880,
		SSL_TIMEOUT = -0x6800,
		SSL_CLIENT_RECONNECT = -0x6780,
		SSL_UNEXPECTED_RECORD = -0x6700,
		SSL_NON_FATAL = -0x6680,
		SSL_ILLEGAL_PARAMETER = -0x6600,
		SSL_CONTINUE_PROCESSING = -0x6580,
		SSL_ASYNC_IN_PROGRESS = -0x6500,
		SSL_EARLY_MESSAGE = -0x6480,
		SSL_UNEXPECTED_CID = -0x6000,
		SSL_VERSION_MISMATCH = -0x5F00,
		SSL_BAD_CONFIG = -0x5E80,
		
		// X509
		X509_FEATURE_UNAVAILABLE = -0x2080,
		X509_UNKNOWN_OID = -0x2100,
		X509_INVALID_FORMAT = -0x2180,
		X509_INVALID_VERSION = -0x2200,
		X509_INVALID_SERIAL = -0x2280,
		X509_INVALID_ALG = -0x2300,
		X509_INVALID_NAME = -0x2380,
		X509_INVALID_DATE = -0x2400,
		X509_INVALID_SIGNATURE = -0x2480,
		X509_INVALID_EXTENSIONS = -0x2500,
		X509_UNKNOWN_VERSION = -0x2580,
		X509_UNKNOWN_SIG_ALG = -0x2600,
		X509_SIG_MISMATCH = -0x2680,
		X509_CERT_VERIFY_FAILED = -0x2700,
		X509_CERT_UNKNOWN_FORMAT = -0x2780,
		X509_BAD_INPUT_DATA = -0x2800,
		X509_ALLOC_FAILED = -0x2880,
		X509_FILE_IO_ERROR = -0x2900,
		X509_BUFFER_TOO_SMALL = -0x2980,
		X509_FATAL_ERROR = -0x3000,

		// Socket errors
		MBEDTLS_ERR_NET_SOCKET_FAILED = -0x0042,
		MBEDTLS_ERR_NET_CONNECT_FAILED = -0x0044,
		MBEDTLS_ERR_NET_BIND_FAILED = -0x0046,
		MBEDTLS_ERR_NET_LISTEN_FAILED = -0x0048,
		MBEDTLS_ERR_NET_ACCEPT_FAILED = -0x004A,
		MBEDTLS_ERR_NET_RECV_FAILED = -0x004C,
		MBEDTLS_ERR_NET_SEND_FAILED = -0x004E,
		MBEDTLS_ERR_NET_CONN_RESET = -0x0050,
		MBEDTLS_ERR_NET_UNKNOWN_HOST = -0x0052,
		MBEDTLS_ERR_NET_BUFFER_TOO_SMALL = -0x0043,
		MBEDTLS_ERR_NET_INVALID_CONTEXT = -0x0045,
		MBEDTLS_ERR_NET_POLL_FAILED = -0x0047,
		MBEDTLS_ERR_NET_BAD_INPUT_DATA = -0x0049,
	}

	public static class MbedTlsSslErrorExtensions
	{
		private static readonly Dictionary<MbedTlsSslError, string> ErrorStrings = new Dictionary<MbedTlsSslError, string>
		{
			{ MbedTlsSslError.SSL_CRYPTO_IN_PROGRESS, "A cryptographic operation is in progress. Try again later." },
			{ MbedTlsSslError.SSL_FEATURE_UNAVAILABLE, "The requested feature is not available." },
			{ MbedTlsSslError.SSL_BAD_INPUT_DATA, "Bad input parameters to function." },
			{ MbedTlsSslError.SSL_INVALID_MAC, "Verification of the message MAC failed." },
			{ MbedTlsSslError.SSL_INVALID_RECORD, "An invalid SSL record was received." },
			{ MbedTlsSslError.SSL_CONN_EOF, "The connection indicated an EOF." },
			{ MbedTlsSslError.SSL_DECODE_ERROR, "A message could not be parsed due to a syntactic error." },
			{ MbedTlsSslError.SSL_NO_RNG, "No RNG was provided to the SSL module." },
			{ MbedTlsSslError.SSL_NO_CLIENT_CERTIFICATE, "No client certification received from the client, but required by the authentication mode." },
			{ MbedTlsSslError.SSL_UNSUPPORTED_EXTENSION, "Client received an extended server hello containing an unsupported extension." },
			{ MbedTlsSslError.SSL_NO_APPLICATION_PROTOCOL, "No ALPN protocols supported that the client advertises." },
			{ MbedTlsSslError.SSL_PRIVATE_KEY_REQUIRED, "The own private key or pre-shared key is not set, but needed." },
			{ MbedTlsSslError.SSL_CA_CHAIN_REQUIRED, "No CA Chain is set, but required to operate." },
			{ MbedTlsSslError.SSL_UNEXPECTED_MESSAGE, "An unexpected message was received from our peer." },
			{ MbedTlsSslError.SSL_FATAL_ALERT_MESSAGE, "A fatal alert message was received from our peer." },
			{ MbedTlsSslError.SSL_UNRECOGNIZED_NAME, "No server could be identified matching the client's SNI." },
			{ MbedTlsSslError.SSL_PEER_CLOSE_NOTIFY, "The peer notified us that the connection is going to be closed." },
			{ MbedTlsSslError.SSL_BAD_CERTIFICATE, "Processing of the Certificate handshake message failed." },
			{ MbedTlsSslError.SSL_ALLOC_FAILED, "Memory allocation failed." },
			{ MbedTlsSslError.SSL_HW_ACCEL_FAILED, "Hardware acceleration function returned with error." },
			{ MbedTlsSslError.SSL_HW_ACCEL_FALLTHROUGH, "Hardware acceleration function skipped / left alone data." },
			{ MbedTlsSslError.SSL_BAD_PROTOCOL_VERSION, "Handshake protocol not within min/max boundaries." },
			{ MbedTlsSslError.SSL_HANDSHAKE_FAILURE, "The handshake negotiation failed." },
			{ MbedTlsSslError.SSL_SESSION_TICKET_EXPIRED, "Session ticket has expired." },
			{ MbedTlsSslError.SSL_PK_TYPE_MISMATCH, "Public key type mismatch (eg, asked for RSA key exchange and presented EC key)." },
			{ MbedTlsSslError.SSL_UNKNOWN_IDENTITY, "Unknown identity received (eg, PSK identity)." },
			{ MbedTlsSslError.SSL_INTERNAL_ERROR, "Internal error (eg, unexpected failure in lower-level module)." },
			{ MbedTlsSslError.SSL_COUNTER_WRAPPING, "A counter would wrap (eg, too many messages exchanged)." },
			{ MbedTlsSslError.SSL_WAITING_SERVER_HELLO_RENEGO, "Unexpected message at ServerHello in renegotiation." },
			{ MbedTlsSslError.SSL_HELLO_VERIFY_REQUIRED, "DTLS client must retry for hello verification." },
			{ MbedTlsSslError.SSL_BUFFER_TOO_SMALL, "A buffer is too small to receive or write a message." },
			{ MbedTlsSslError.SSL_WANT_READ, "No data of requested type currently available on underlying transport." },
			{ MbedTlsSslError.SSL_WANT_WRITE, "Connection requires a write call." },
			{ MbedTlsSslError.SSL_TIMEOUT, "The operation timed out." },
			{ MbedTlsSslError.SSL_CLIENT_RECONNECT, "The client initiated a reconnect from the same port." },
			{ MbedTlsSslError.SSL_UNEXPECTED_RECORD, "Record header looks valid but is not expected." },
			{ MbedTlsSslError.SSL_NON_FATAL, "The alert message received indicates a non-fatal error." },
			{ MbedTlsSslError.SSL_ILLEGAL_PARAMETER, "A field in a message was incorrect or inconsistent with other fields." },
			{ MbedTlsSslError.SSL_CONTINUE_PROCESSING, "Internal-only message signaling that further message-processing should be done." },
			{ MbedTlsSslError.SSL_ASYNC_IN_PROGRESS, "The asynchronous operation is not completed yet." },
			{ MbedTlsSslError.SSL_EARLY_MESSAGE, "Internal-only message signaling that a message arrived early." },
			{ MbedTlsSslError.SSL_UNEXPECTED_CID, "An encrypted DTLS-frame with an unexpected CID was received." },
			{ MbedTlsSslError.SSL_VERSION_MISMATCH, "An operation failed due to an unexpected version or configuration." },
			{ MbedTlsSslError.SSL_BAD_CONFIG, "Invalid value in SSL config." },
			{ MbedTlsSslError.X509_FEATURE_UNAVAILABLE, "Unavailable feature, e.g. RSA hashing/encryption combination." },
			{ MbedTlsSslError.X509_UNKNOWN_OID, "Requested OID is unknown." },
			{ MbedTlsSslError.X509_INVALID_FORMAT, "The CRT/CRL/CSR format is invalid, e.g. different type expected." },
			{ MbedTlsSslError.X509_INVALID_VERSION, "The CRT/CRL/CSR version element is invalid." },
			{ MbedTlsSslError.X509_INVALID_SERIAL, "The serial tag or value is invalid." },
			{ MbedTlsSslError.X509_INVALID_ALG, "The algorithm tag or value is invalid." },
			{ MbedTlsSslError.X509_INVALID_NAME, "The name tag or value is invalid." },
			{ MbedTlsSslError.X509_INVALID_DATE, "The date tag or value is invalid." },
			{ MbedTlsSslError.X509_INVALID_SIGNATURE, "The signature tag or value invalid." },
			{ MbedTlsSslError.X509_INVALID_EXTENSIONS, "The extension tag or value is invalid." },
			{ MbedTlsSslError.X509_UNKNOWN_VERSION, "CRT/CRL/CSR has an unsupported version number." },
			{ MbedTlsSslError.X509_UNKNOWN_SIG_ALG, "Signature algorithm (oid) is unsupported." },
			{ MbedTlsSslError.X509_SIG_MISMATCH, "Signature algorithms do not match." },
			{ MbedTlsSslError.X509_CERT_VERIFY_FAILED, "Certificate verification failed, e.g. CRL, CA or signature check failed." },
			{ MbedTlsSslError.X509_CERT_UNKNOWN_FORMAT, "Format not recognized as DER or PEM." },
			{ MbedTlsSslError.X509_BAD_INPUT_DATA, "Input invalid." },
			{ MbedTlsSslError.X509_ALLOC_FAILED, "Allocation of memory failed." },
			{ MbedTlsSslError.X509_FILE_IO_ERROR, "Read/write of file failed." },
			{ MbedTlsSslError.X509_BUFFER_TOO_SMALL, "Destination buffer is too small." },
			{ MbedTlsSslError.X509_FATAL_ERROR, "A fatal error occurred, eg the chain is too long or the vrfy callback failed." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_SOCKET_FAILED, "Failed to open a socket." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_CONNECT_FAILED, "The connection to the given server / port failed." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_BIND_FAILED, "Binding of the socket failed." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_LISTEN_FAILED, "Could not listen on the socket." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_ACCEPT_FAILED, "Could not accept the incoming connection." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_RECV_FAILED, "Reading information from the socket failed." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_SEND_FAILED, "Sending information through the socket failed." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_CONN_RESET, "Connection was reset by peer." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_UNKNOWN_HOST, "Failed to get an IP address for the given hostname." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_BUFFER_TOO_SMALL, "Buffer is too small to hold the data." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_INVALID_CONTEXT, "The context is invalid, eg because it was free()ed." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_POLL_FAILED, "Polling the net context failed." },
			{ MbedTlsSslError.MBEDTLS_ERR_NET_BAD_INPUT_DATA, "Input invalid." },

		};

		public static string ToErrorString(this MbedTlsSslError error)
		{
			return ErrorStrings.TryGetValue(error, out var value) ? value : $"Unknown error code: {error}";
		}
	}
}
#endif