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

        //native resources
        IntPtr native_context;
        IntPtr read_buf;
        IntPtr write_buf;
        bool isAuthenticated;

        const int buffer_size = 4096;

        public MbedTlsContext (MNS.MobileAuthenticatedStream mas_stream, MNS.MonoSslAuthenticationOptions options, IntPtr mono_fd, NetworkStream network_stream)
            : base (mas_stream, options)
        {
            //create I/O buffers and give the to mbedTLS
            read_buf = Marshal.AllocHGlobal (buffer_size);
            write_buf = Marshal.AllocHGlobal (buffer_size);
            Console.WriteLine("calling mono_mbedtls_init");
            Thread.Sleep(100);

            mono_mbedtls_init (mono_fd, read_buf, write_buf);
        }

        public override void StartHandshake ()
        {
            throw new NotImplementedException ();
        }

        public override void Flush ()
		{
			throw new NotImplementedException ();
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
			throw new NotSupportedException ();
		}

		public override bool PendingRenegotiation ()
		{
			throw new NotSupportedException ();
		}

        public override (int ret, bool wantMore) Read (byte[] buffer, int offset, int size)
        {
            throw new NotSupportedException ();
        }

        public override (int ret, bool wantMore) Write (byte[] buffer, int offset, int size)
        {
            throw new NotSupportedException ();
        }

        public override void FinishHandshake ()
		{
            throw new NotSupportedException ();
		}

        public override bool HasContext {
			get { throw new NotSupportedException (); }
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
            throw new NotSupportedException ();
        }
    }

}
#endif