//
// System.Net.NetworkInformation.IPInterfaceProperties
//
// Authors:
//	Gonzalo Paniagua Javier (gonzalo@novell.com)
//	Atsushi Enomoto (atsushi@ximian.com)
//
// Copyright (c) 2006-2007 Novell, Inc. (http://www.novell.com)
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
using System.Collections.Generic;
using System.Net.Sockets;
using System.IO;
using System.Text.RegularExpressions;
using System.Runtime.InteropServices;

namespace System.Net.NetworkInformation {
	internal class NuttxIPInterfaceProperties : IPInterfaceProperties
	{
		protected IPv4InterfaceProperties _ipv4iface_properties;
		protected NuttxNetworkInterface _iface;
		List<IPAddress> _addresses;
		IPAddressCollection _dns_servers;

		public NuttxIPInterfaceProperties(NuttxNetworkInterface iface, List <IPAddress> addresses, IPAddressCollection dns_servers)
		{
			_iface = iface;
			_addresses = addresses;
			_dns_servers = dns_servers;
		}

		public override IPv4InterfaceProperties GetIPv4Properties()
		{
			if (_ipv4iface_properties == null)
			{
				_ipv4iface_properties = new NuttxIPv4InterfaceProperties(_iface as NuttxNetworkInterface);
			}

			return _ipv4iface_properties;
		}

		public override IPv6InterfaceProperties GetIPv6Properties()
		{
			throw new NotImplementedException(nameof(GetIPv6Properties));
		}

		void GetDNSServersFromOS()
		{
			try
			{
				_dns_servers = new IPAddressCollection();

				string filePath = Path.Combine("meadow0", "dns.conf");

				if (File.Exists(filePath))
				{
					string line = File.ReadAllText(filePath).Trim();

					if (!string.IsNullOrEmpty(line) && line.StartsWith("nameserver"))
					{
						string[] elements = line.Split(new string[] { "nameserver" }, StringSplitOptions.RemoveEmptyEntries);

						foreach (string element in elements)
						{
							string ipAddress = element.Trim();

							// Convert the string IP address to IPAddress
							if (IPAddress.TryParse(ipAddress, out IPAddress dnsServer))
							{
								_dns_servers.InternalAdd(dnsServer);
							}
							else
							{
								throw new FormatException($"Invalid IP address format in line: {line}");
							}
						}
					}
					else
					{
						throw new FormatException($"Invalid format in the dns.conf file: {line}");
					}
				}
				else
				{
					throw new FileNotFoundException($"File not found: {filePath}");
				}
			}
			catch (Exception ex)
			{
				Console.WriteLine($"Error reading dns.conf: {ex.Message}");
			}
		}
		[DllImport("nuttx", EntryPoint="meadow_os_network_interface_info")]
		public static extern int meadow_os_network_interface_info(IntPtr buffer);

		internal static unsafe string GetNetWorkInterfaceInfo()
		{
			var buffer = Marshal.AllocHGlobal(125);
			string info = null;
			try
			{
				int len = meadow_os_network_interface_info(buffer);
				if (len > 0)
				{
					info = System.Text.Encoding.UTF8.GetString((byte*)buffer.ToPointer(), len);
				}
			}
			finally
			{
				Marshal.FreeHGlobal(buffer);
			}
			return info;
		}
		IPAddressCollection ParseRouteInfo()
		{
			var col = new IPAddressCollection();
			try
			{
				IPAddress gwAddr = IPAddress.Parse(GetNetWorkInterfaceInfo());
				col.InternalAdd(gwAddr);
			}catch{
			}
			return col;
		}

		public override GatewayIPAddressInformationCollection GatewayAddresses
		{
			get
			{
				return SystemGatewayIPAddressInformation.ToGatewayIpAddressInformationCollection(ParseRouteInfo());
			}
		}


		public override IPAddressInformationCollection AnycastAddresses
		{
			get
			{
				var c = new IPAddressInformationCollection();
				foreach (IPAddress address in _addresses)
				{
					c.InternalAdd(new SystemIPAddressInformation(address, false, false));
				}
				return c;
			}
		}

		[MonoTODO ("Always returns an empty collection.")]
		public override IPAddressCollection DhcpServerAddresses
		{
			get { return new IPAddressCollection(); }
		}

		public override IPAddressCollection DnsAddresses
		{
			get
			{ 
				GetDNSServersFromOS();
				return _dns_servers;
			}
		}

		public override string DnsSuffix
		{
			get { return String.Empty; }
		}

		[MonoTODO ("Always returns false")]
		public override bool IsDnsEnabled
		{
			get { return true; }
		}

		[MonoTODO ("Always returns false")]
		public override bool IsDynamicDnsEnabled
		{
			get { return false; }
		}

		public override MulticastIPAddressInformationCollection MulticastAddresses
		{
			get
			{
				var multicastAddresses = new MulticastIPAddressInformationCollection();
				foreach (IPAddress address in _addresses)
				{
					byte[] addressBytes = address.GetAddressBytes();
					if ((addressBytes[0] >= 224) && (addressBytes[0] <= 239))
					{
						multicastAddresses.InternalAdd(new SystemMulticastIPAddressInformation(new SystemIPAddressInformation(address, true, false)));
					}
				}
				return multicastAddresses;
			}
		}

		public override UnicastIPAddressInformationCollection UnicastAddresses 
		{
			get
			{
				var unicastAddresses = new UnicastIPAddressInformationCollection();
				foreach (IPAddress address in _addresses)
				{
					switch (address.AddressFamily)
					{
						case AddressFamily.InterNetwork:
							byte top = address.GetAddressBytes()[0];
							if ((top >= 224) && (top <= 239))
							{
								continue;
							}
							unicastAddresses.InternalAdd(new LinuxUnicastIPAddressInformation(address));
							break;
						case AddressFamily.InterNetworkV6:
							if (address.IsIPv6Multicast)
							{
								continue;
							}
							unicastAddresses.InternalAdd(new LinuxUnicastIPAddressInformation(address));
							break;
					}
				}
				return unicastAddresses;
			}
		}

		[MonoTODO ("Always returns an empty collection.")]
		public override IPAddressCollection WinsServersAddresses
		{
			get { return new IPAddressCollection(); }
		}
	}
}
