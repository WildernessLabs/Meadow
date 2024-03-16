#if SECURITY_DEP && MONO_FEATURE_MBEDTLS
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
		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_connect")]
		internal static extern IntPtr mono_mbedtls_connect(IntPtr fd, IntPtr read_buf, IntPtr write_buf, string hostname);

		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_read")]
		internal static extern int mono_mbedtls_read(IntPtr ctx, int length);

		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_write")]
		internal static extern int mono_mbedtls_write(IntPtr ctx, int length);

		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_close")]
		internal static extern int mono_mbedtls_close(IntPtr ctx);

		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_init")]
		internal static extern int mono_mbedtls_init();

		[DllImport("mbedtls", EntryPoint = "mono_mbedtls_handshake")]
		internal static extern int mono_mbedtls_handshake(IntPtr ctx);

		//Managed resources
		SafeHandle socket_handle;
		bool socket_release;

		//native resources
		IntPtr native_context;
		IntPtr read_buf;
		IntPtr write_buf;
		bool isAuthenticated;
		bool disposed;
	 bool closed;

		const int buffer_size = 4096;

		static MbedTlsContext ()
		{
			int initResult = mono_mbedtls_init();
			if (initResult != 0)
				throw new IOException($"TLS initialization failed with error code: {initResult}");
		}

		public MbedTlsContext (MNS.MobileAuthenticatedStream mas_stream, MNS.MonoSslAuthenticationOptions options, SafeHandle socket_handle, NetworkStream network_stream)
			: base (mas_stream, options)
		{
			this.socket_handle = socket_handle;
			socket_handle.DangerousAddRef (ref socket_release);
			if (!socket_release)
				throw new IOException ("Could not add a reference to underlying socket");
			IntPtr mono_fd = socket_handle.DangerousGetHandle ();
			if (mono_fd == IntPtr.Zero)
				throw new InvalidOperationException("Invalid socket handle");
			//create I/O buffers and give the to mbedTLS
			read_buf = Marshal.AllocHGlobal (buffer_size);
			write_buf = Marshal.AllocHGlobal (buffer_size);
			string hostname = network_stream._streamSocket.hostname;

			native_context = mono_mbedtls_connect (mono_fd, read_buf, write_buf, hostname);

			if (native_context == IntPtr.Zero)
			{
				throw new IOException ("TLS connection failed");
			}
		}

		public override void StartHandshake ()
		{
			int ret = mono_mbedtls_handshake(native_context);
			if (ret != 0)
				throw new IOException ("Handshake failed");
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
				closed = true;
		}

		public override bool PendingRenegotiation ()
		{
			throw new NotSupportedException ();
		}

		public override (int ret, bool wantMore) Read (byte[] buffer, int offset, int size)
		{
			if (disposed)
				throw  new ObjectDisposedException ("TLS Context was disposed.");

			if (closed)
				return (0, false);

			if (size > buffer_size)
				size = buffer_size;

			int ret = mono_mbedtls_read (native_context, size);

			// It means that the SSL/TLS read operation completed successfully,
			// but the peer has indicated that there is no more data to read. 
			if (ret == 1)
				return (size, false);
			if (ret > 0) {
				Marshal.Copy (read_buf, buffer, offset, ret);
			}
			else
				ret = 0;

			return (ret, false);
		}

		public override (int ret, bool wantMore) Write (byte[] buffer, int offset, int size)
		{
			if (disposed)
				throw  new ObjectDisposedException ("TLS Context was disposed.");

			if (size > buffer_size)
				size = buffer_size;

			Marshal.Copy (buffer, offset, write_buf, size);
			int ret = mono_mbedtls_write (native_context, size);

			return (ret, false);
		}

		public override void FinishHandshake ()
		{
			isAuthenticated = true;
		}

		public override bool HasContext {
				get { return !disposed && native_context != IntPtr.Zero; }
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

		protected override void Dispose (bool disposing)
		{
			if (disposed)
				return;
			try {
				mono_mbedtls_close (native_context);
			}
			finally {
				disposed = true;
				if (socket_release)
					socket_handle.DangerousRelease();
				var tmp = read_buf;
				read_buf = IntPtr.Zero;
				Marshal.FreeHGlobal (tmp);
				tmp = write_buf;
				write_buf = IntPtr.Zero;
				Marshal.FreeHGlobal (tmp);
				base.Dispose (disposing);
			}
		}
	}

}
#endif
