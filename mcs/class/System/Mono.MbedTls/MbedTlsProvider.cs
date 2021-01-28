#if SECURITY_DEP && MONO_FEATURE_MBEDTLS
#if MONO_SECURITY_ALIAS
extern alias MonoSecurity;
#endif

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Net.Security;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Security.Authentication;
using Microsoft.Win32.SafeHandles;

#if MONO_SECURITY_ALIAS
using MonoSecurity::Mono.Security.Interface;
using MX = MonoSecurity::Mono.Security.X509;
#else
using Mono.Security.Interface;
using MX = Mono.Security.X509;
#endif

using MNS = Mono.Net.Security;

namespace Mono.MbedTls
{
	class MbedTlsProvider : MNS.MobileTlsProvider
	{
		public override Guid ID {
			get { return MNS.MonoTlsProviderFactory.MbedTlsId; }
		}
		public override string Name {
			get { return "mbedtls"; }
		}

		internal MbedTlsProvider ()
		{

		}

		public override bool SupportsSslStream {
			get { return true; }
		}

		public override bool SupportsMonoExtensions {
			get { return false; }
		}

		public override bool SupportsConnectionInfo {
			get { return true; }
		}

		internal override bool SupportsCleanShutdown {
			get { return true; }
		}

		public override SslProtocols SupportedProtocols {
			get { return SslProtocols.Tls12 | SslProtocols.Tls11 | SslProtocols.Tls; }
		}

		internal override MNS.MobileAuthenticatedStream CreateSslStream (
			SslStream sslStream, Stream innerStream, bool leaveInnerStreamOpen,
			MonoTlsSettings settings)
		{
			return new MbedTlsStream (
				innerStream, leaveInnerStreamOpen, sslStream, settings, this);
		}

		internal override bool HasNativeCertificates {
			get { return true; }
		}

		internal X509Certificate2Impl GetNativeCertificate (
			byte[] data, string password, X509KeyStorageFlags flags)
		{
			using (var handle = new SafePasswordHandle (password))
				return GetNativeCertificate (data, handle, flags);
		}

		internal X509Certificate2Impl GetNativeCertificate (
			X509Certificate certificate)
		{
			throw new NotImplementedException();
		}

		internal X509Certificate2Impl GetNativeCertificate (
			byte[] data, SafePasswordHandle password, X509KeyStorageFlags flags)
		{
			throw new NotImplementedException();
		}

		internal override bool ValidateCertificate (
			MNS.ChainValidationHelper validator, string targetHost, bool serverMode,
			X509CertificateCollection certificates, bool wantsChain, ref X509Chain chain,
			ref SslPolicyErrors errors, ref int status11)
		{
			return true;
		}


	}
}
#endif