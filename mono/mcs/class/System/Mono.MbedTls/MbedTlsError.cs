#if SECURITY_DEP && MONO_FEATURE_MBEDTLS
#if MONO_SECURITY_ALIAS
extern alias MonoSecurity;
#endif

namespace Mono.MbedTls
{
	enum MbedTlsError
	{
		None = 0,
		ZeroReturn = 1, // The peer has indicated that there is no more data to read
	}
}
#endif
