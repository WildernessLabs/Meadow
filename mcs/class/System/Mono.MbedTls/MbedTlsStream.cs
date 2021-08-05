#if SECURITY_DEP && MONO_FEATURE_MBEDTLS
#if MONO_SECURITY_ALIAS
extern alias MonoSecurity;
#endif

using System;
using System.IO;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography.X509Certificates;
using System.Runtime.InteropServices;

#if MONO_SECURITY_ALIAS
using MonoSecurity::Mono.Security.Interface;
#else
using Mono.Security.Interface;
#endif

using MNS = Mono.Net.Security;

namespace Mono.MbedTls
{
	class MbedTlsStream : MNS.MobileAuthenticatedStream
	{
		NetworkStream network_stream;

		public MbedTlsStream (Stream innerStream, bool leaveInnerStreamOpen, SslStream owner,
		                       MonoTlsSettings settings, MNS.MobileTlsProvider provider)
			: base (innerStream, leaveInnerStreamOpen, owner, settings, provider)
		{
			network_stream = innerStream as NetworkStream;
		}

		protected override MNS.MobileTlsContext CreateContext (MNS.MonoSslAuthenticationOptions options)
		{
			return new MbedTlsContext (this, options, network_stream._streamSocket.SafeHandle, network_stream);
		}

	}
}
#endif
