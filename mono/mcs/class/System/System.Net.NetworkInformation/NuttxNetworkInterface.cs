//
// System.Net.NetworkInformation.NetworkInterface
//
// Authors:
//	Gonzalo Paniagua Javier (gonzalo@novell.com)
//	Atsushi Enomoto (atsushi@ximian.com)
//      Miguel de Icaza (miguel@novell.com)
//      Eric Butler (eric@extremeboredom.net)
//      Marek Habersack (mhabersack@novell.com)
//  Marek Safar (marek.safar@gmail.com)
//
// Copyright (c) 2006-2008 Novell, Inc. (http://www.novell.com)
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
using System.Runtime.InteropServices;
using System.Net.Sockets;

namespace System.Net.NetworkInformation
{
	internal class NuttxNetworkInterfaceAPI : NetworkInterfaceFactory
	{
		const int AF_INET = 2;
		protected readonly int AF_INET6;

		private const string PppInterfacePrefix = "ppp";
		private const string LoopbackInterfacePrefix = "lo";

		public NuttxNetworkInterfaceAPI ()
		{
			AF_INET6 = 10;
		}

		protected NuttxNetworkInterfaceAPI (int AF_INET6)
		{
			this.AF_INET6 = AF_INET6;
		}

		[DllImport("nuttx", EntryPoint="nx_if_nametoindex")]
		public static extern int if_nametoindex(string ifname);

		[DllImport("nuttx", EntryPoint="nx_getifaddrs")]
		protected static extern int getifaddrs (out IntPtr ifap);

		[DllImport("nuttx", EntryPoint="nx_freeifaddrs")]
		protected static extern void freeifaddrs (IntPtr ifap);

		public override NetworkInterface[] GetAllNetworkInterfaces()
		{
			var interfaces = new Dictionary <string, NuttxNetworkInterface>();
			IntPtr ifap;
			if (getifaddrs(out ifap) != 0)
			{
				throw new SystemException("Unable to read network information from Meadow OS");
			}	

			try
			{
				IntPtr next = ifap;
				while (next != IntPtr.Zero)
				{
					NuttxStructs.ifaddrs addr = (NuttxStructs.ifaddrs) Marshal.PtrToStructure(next, typeof (NuttxStructs.ifaddrs));
					IPAddress address = IPAddress.None;
					string    name = addr.ifa_name;
					int       index = -1;
					byte[]    macAddress = null;
					NetworkInterfaceType type = NetworkInterfaceType.Unknown;

					if (addr.ifa_addr != IntPtr.Zero)
					{
						NuttxStructs.sockaddr sockaddr = (NuttxStructs.sockaddr) Marshal.PtrToStructure(addr.ifa_addr, typeof(NuttxStructs.sockaddr));

						// if (sockaddr.sa_family == AF_INET6) {
						// 	NuttxStructs.sockaddr_in6 sockaddr6 = (NuttxStructs.sockaddr_in6) Marshal.PtrToStructure (addr.ifa_addr, typeof (NuttxStructs.sockaddr_in6));
						// 	address = new IPAddress (sockaddr6.sin6_addr.u6_addr8, sockaddr6.sin6_scope_id);
						// } else
						if (sockaddr.sa_family == AF_INET)
						{
							NuttxStructs.sockaddr_in sockaddrin = (NuttxStructs.sockaddr_in) Marshal.PtrToStructure(addr.ifa_addr, typeof(NuttxStructs.sockaddr_in));
							address = new IPAddress(sockaddrin.sin_addr);
							if (addr.ifa_data != IntPtr.Zero)
							{
								NuttxStructs.macaddress hardwareAddress = (NuttxStructs.macaddress) Marshal.PtrToStructure(addr.ifa_data, typeof(NuttxStructs.macaddress));
								macAddress = new byte[] { 0, 0, 0, 0, 0, 0 };
								Array.Copy(hardwareAddress.address, macAddress, 6);
							}
						}
						if ((addr.ifa_flags & NuttxStructs.NuttxInterfaceFlags.IFF_WIFI) == NuttxStructs.NuttxInterfaceFlags.IFF_WIFI)
						{
							type = NetworkInterfaceType.Wireless80211;
							addr.ifa_flags &= ~NuttxStructs.NuttxInterfaceFlags.IFF_WIFI;	// The IFF_WIFI flag is Meadow specific so remove it.
						}
						else
						{
							type = NetworkInterfaceType.Ethernet;
						}
					}
					NuttxNetworkInterface iface = null;

					// create interface if not already present
					if (!interfaces.TryGetValue (name, out iface))
					{
						iface = new NuttxNetworkInterface (name, addr.ifa_flags);
						interfaces.Add(name, iface);
					}

					// if a new address has been found, add it
					if (!address.Equals(IPAddress.None))
					{
						iface.AddAddress(address);
					}

					if (name.Contains(PppInterfacePrefix))
					{
						type = NetworkInterfaceType.Ppp;
					}
					else if (name.Contains(LoopbackInterfacePrefix))
					{
						type = NetworkInterfaceType.Loopback;
					}

					// set link layer info, if iface has macaddress, is loopback device or it represents a PPP connection
					if ((macAddress != null) || (type == NetworkInterfaceType.Loopback) || (type == NetworkInterfaceType.Ppp))
					{
						iface.SetLinkLayerInfo(index, macAddress, type);
					}
					next = addr.ifa_next;
				}
			}
			finally
			{
				freeifaddrs(ifap);
			}

			NetworkInterface[] result = new NetworkInterface[interfaces.Count];
			int x = 0;
			foreach (NetworkInterface thisInterface in interfaces.Values)
			{
				result [x] = thisInterface;
				x++;
			}
			return result;
		}

		public override int GetLoopbackInterfaceIndex()
		{
			return(0);
			// return if_nametoindex ("lo0");
		}

		public override IPAddress GetNetMask (IPAddress address)
		{
			IntPtr ifap;
			if (getifaddrs(out ifap) != 0)
			{
				throw new SystemException ("Unable to read network information from Meadow OS");
			}

			try
			{
				IntPtr next = ifap;
				while (next != IntPtr.Zero)
				{
					NuttxStructs.ifaddrs addr = (NuttxStructs.ifaddrs) Marshal.PtrToStructure(next, typeof (NuttxStructs.ifaddrs));
					if (addr.ifa_addr != IntPtr.Zero)
					{
						NuttxStructs.sockaddr sockaddr = (NuttxStructs.sockaddr) Marshal.PtrToStructure(addr.ifa_addr, typeof (NuttxStructs.sockaddr));
						if (sockaddr.sa_family == AF_INET)
						{
							NuttxStructs.sockaddr_in sockaddrin = (NuttxStructs.sockaddr_in) Marshal.PtrToStructure(addr.ifa_addr, typeof (NuttxStructs.sockaddr_in));
							var saddress = new IPAddress (sockaddrin.sin_addr);
							if (address.Equals (saddress))
							{
								return new IPAddress(((sockaddr_in) Marshal.PtrToStructure(addr.ifa_netmask, typeof(sockaddr_in))).sin_addr);
							}
						}
					}
					next = addr.ifa_next;
				}
			}
			finally
			{
				freeifaddrs(ifap);
			}
			return null;
		}
	}

	sealed class NuttxNetworkInterface : NetworkInterface
	{
		// private IPv4InterfaceStatistics ipv4stats;
		private IPInterfaceProperties _ipproperties;
		private List<IPAddress> _addresses;

		string _name;
		byte[]               _macAddress;
		NetworkInterfaceType _type;

		private uint _ifa_flags;

		internal NuttxNetworkInterface(string name, uint ifa_flags)
		{
			_name = name;
			_ifa_flags = ifa_flags;
			_type = NetworkInterfaceType.Unknown;
			_addresses = new List<IPAddress>();
		}

		internal void AddAddress(IPAddress address)
		{
			_addresses.Add(address);
		}

		internal void SetLinkLayerInfo(int index, byte[] macAddress, NetworkInterfaceType type)
		{
			//this.index = index;
			_macAddress = macAddress;
			_type = type;
		}


		public override IPInterfaceProperties GetIPProperties()
		{
			if (_ipproperties == null)
			{
				_ipproperties = new NuttxIPInterfaceProperties(this, _addresses);
			}
			return _ipproperties;
		}

		// public override IPv4InterfaceStatistics GetIPv4Statistics()
		// {
		// 	if (ipv4stats == null)
		// 		ipv4stats = new NuttxIPv4InterfaceStatistics (this);
		// 	return ipv4stats;
		// }

		public override OperationalStatus OperationalStatus
		{
			get
			{
				if ((_ifa_flags & NuttxStructs.NuttxInterfaceFlags.IFF_UP) == NuttxStructs.NuttxInterfaceFlags.IFF_UP)
				{
					return OperationalStatus.Up;
				}
				return OperationalStatus.Unknown;
			}
		}

		public override bool SupportsMulticast
		{
			get { return false; }
		}

		public override PhysicalAddress GetPhysicalAddress()
		{
			if (_macAddress != null)
			{
				return new PhysicalAddress(_macAddress);
			}
			else
			{
				return PhysicalAddress.None;
			}
		}

		public override bool Supports(NetworkInterfaceComponent networkInterfaceComponent)
		{
			bool wantIPv4 = networkInterfaceComponent == NetworkInterfaceComponent.IPv4;
			bool wantIPv6 = wantIPv4 ? false : networkInterfaceComponent == NetworkInterfaceComponent.IPv6;

			foreach (IPAddress address in _addresses)
			{
				if (wantIPv4 && address.AddressFamily == AddressFamily.InterNetwork)
				{
					return true;
				}
				else 
				{
					if (wantIPv6 && address.AddressFamily == AddressFamily.InterNetworkV6)
					{
						return true;
					}
				}
			}
			return false;
		}

		public override string Description
		{
			get { return _name; }
		}

		public override string Id
		{
			get { return _name; }
		}

		public override bool IsReceiveOnly
		{
			get { return false; }
		}

		public override string Name
		{
			get { return _name; }
		}

		public override NetworkInterfaceType NetworkInterfaceType
		{
			get { return _type; }
		}

		public override long Speed
		{
			get { return 1000000; }			// Bits/s
		}

		internal int NameIndex
		{
			get { return NuttxNetworkInterfaceAPI.if_nametoindex (Name); }
		}
	}
}
