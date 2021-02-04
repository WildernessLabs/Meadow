#if SECURITY_DEP && MONO_FEATURE_BTLS
#if MONO_SECURITY_ALIAS
extern alias MonoSecurity;
#endif

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Security.Cryptography.X509Certificates;
using System.Security.Authentication;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
using System.Net.Security;
using System.Net.Sockets;

#if MONO_SECURITY_ALIAS
using MonoSecurity::Mono.Security.Interface;
#else
using Mono.Security.Interface;
#endif

using MNS = Mono.Net.Security;

namespace Mono.MbedTls
{
	class MbedTlsContext : MNS.MobileTlsContext
	{
		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_init")]
		internal static extern IntPtr mono_mbedtls_init(IntPtr fd, IntPtr read_buf, IntPtr write_buf);

		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_read")]
		internal static extern int mono_mbedtls_read(IntPtr ctx, int length);

		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_write")]
		internal static extern int mono_mbedtls_write(IntPtr ctx, int length);

		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_close")]
		internal static extern int mono_mbedtls_close(IntPtr ctx);

		//native resources
		IntPtr native_context;
		IntPtr read_buf;
		IntPtr write_buf;
		bool isAuthenticated;

		const int buffer_size = 131072;

		public MbedTlsContext (MNS.MobileAuthenticatedStream mas_stream, MNS.MonoSslAuthenticationOptions options, IntPtr mono_fd, NetworkStream network_stream)
			: base (mas_stream, options)
		{
			//create I/O buffers and give the to mbedTLS
			read_buf = Marshal.AllocHGlobal (buffer_size);
			write_buf = Marshal.AllocHGlobal (buffer_size);

			native_context = mono_mbedtls_init (mono_fd, read_buf, write_buf);

			if (native_context == IntPtr.Zero)
				throw new IOException ("TLS initialization or handshake failed");
			isAuthenticated = true;
		}

		public override void StartHandshake ()
		{
			// we immediately start/complete a handshake on construction of the context
			return;
		}

		public override void Flush ()
		{
			// TODO: Implement
			return;
		}

		public override bool CanRenegotiate {
			get {
				return false;
			}
		}

		public override void Renegotiate ()
		{
			throw new NotSupportedException ();
		}

		public override TlsProtocols NegotiatedProtocol {
			get { throw new NotSupportedException (); }
		}

		public override bool IsAuthenticated {
			get { return isAuthenticated; }
		}

		public override void Shutdown ()
		{
			Marshal.FreeHGlobal (read_buf);
			Marshal.FreeHGlobal (write_buf);
			mono_mbedtls_close (native_context);
			return;
		}

		public override bool PendingRenegotiation ()
		{
			throw new NotSupportedException ();
		}

		public override (int ret, bool wantMore) Read (byte[] buffer, int offset, int size)
		{
			// Console.WriteLine($"Trying to read {size} bytes at {offset}");
			if (size > buffer_size)
				size = buffer_size;
			int ret = mono_mbedtls_read (native_context, size);
			if (ret > 0) {
				Marshal.Copy (read_buf, buffer, offset, ret);
			}
			else
				ret = 0;
			Console.WriteLine(ret);

			return (ret, false);
		}

		public override (int ret, bool wantMore) Write (byte[] buffer, int offset, int size)
		{
			// Console.WriteLine($"Trying to write {size} bytes at {offset}");
			if (size > buffer_size)
				size = buffer_size;

			Marshal.Copy (buffer, offset, write_buf, size);
			int ret = mono_mbedtls_write (native_context, size);
			Console.WriteLine(ret);

			return (ret, false);
		}

		public override void FinishHandshake ()
		{
			// we immediately start/complete a handshake on construction of the context
			return;
		}

		public override bool HasContext {
			get { return true; }
		}

		internal override bool IsRemoteCertificateAvailable {
			get { return false; }
		}

		public override MonoTlsConnectionInfo ConnectionInfo {
			get { throw new NotSupportedException (); }
		}

		public override X509Certificate2 RemoteCertificate {
			get { throw new NotSupportedException (); }
		}

		internal override X509Certificate LocalClientCertificate {
			get { throw new NotSupportedException (); }
		}

		public override bool ProcessHandshake ()
		{
			// we immediately start/complete a handshake on construction of the context
			return true;
		}
	}

}
#endif