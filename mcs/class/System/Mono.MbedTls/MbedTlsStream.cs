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
		IntPtr mono_fd;
		NetworkStream network_stream;
		SafeHandle socket_handle;
		bool release;

		public MbedTlsStream (Stream innerStream, bool leaveInnerStreamOpen, SslStream owner,
		                       MonoTlsSettings settings, MNS.MobileTlsProvider provider)
			: base (innerStream, leaveInnerStreamOpen, owner, settings, provider)
		{
			network_stream = innerStream as NetworkStream;
			socket_handle = network_stream._streamSocket.SafeHandle;
		}

		protected override MNS.MobileTlsContext CreateContext (MNS.MonoSslAuthenticationOptions options)
		{
			Console.WriteLine("Creating TLS context");
			socket_handle.DangerousAddRef (ref release);
			mono_fd = socket_handle.DangerousGetHandle ();
			return new MbedTlsContext (this, options, mono_fd, network_stream);
		}

		protected override void Dispose (bool disposing)
		{
			var socket_handle = network_stream._streamSocket.SafeHandle;
			socket_handle.DangerousRelease();
		}
	}
}
#endif
