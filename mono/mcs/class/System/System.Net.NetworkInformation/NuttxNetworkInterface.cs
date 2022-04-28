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

//	TODO: NX-MS Make this determine the real WiFi capability.

namespace System.Net.NetworkInformation {
	internal class NuttxNetworkInterfaceAPI : NetworkInterfaceFactory
	{
		const int AF_INET = 2;
		protected readonly int AF_INET6;

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
				throw new SystemException ("getifaddrs() failed");
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
						NuttxStructs.sockaddr sockaddr = (NuttxStructs.sockaddr) Marshal.PtrToStructure(addr.ifa_addr, typeof (NuttxStructs.sockaddr));

						// if (sockaddr.sa_family == AF_INET6) {
						// 	NuttxStructs.sockaddr_in6 sockaddr6 = (NuttxStructs.sockaddr_in6) Marshal.PtrToStructure (addr.ifa_addr, typeof (NuttxStructs.sockaddr_in6));
						// 	address = new IPAddress (sockaddr6.sin6_addr.u6_addr8, sockaddr6.sin6_scope_id);
						// } else
						if (sockaddr.sa_family == AF_INET)
						{
							NuttxStructs.sockaddr_in sockaddrin = (NuttxStructs.sockaddr_in) Marshal.PtrToStructure(addr.ifa_addr, typeof (NuttxStructs.sockaddr_in));
							address = new IPAddress (sockaddrin.sin_addr);
						}
						macAddress = new byte [] { 0, 0, 0, 0, 0, 0 };
						// Array.Copy (sockaddrdl.sdl_data, sockaddrdl.sdl_nlen, macAddress, 0, Math.Min (macAddress.Length, sockaddrdl.sdl_data.Length - sockaddrdl.sdl_nlen));
						type = NetworkInterfaceType.Ethernet;
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

					// set link layer info, if iface has macaddress or is loopback device
					if ((macAddress != null) || (type == NetworkInterfaceType.Loopback))
					{
						iface.SetLinkLayerInfo (index, macAddress, type);
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
				throw new SystemException ("getifaddrs() failed");
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
		protected IPv4InterfaceStatistics ipv4stats;
		protected IPInterfaceProperties ipproperties;

		string               name;
		protected List <IPAddress> addresses;
		byte[]               macAddress;
		NetworkInterfaceType type;

		private uint _ifa_flags;

		internal NuttxNetworkInterface(string name, uint ifa_flags)
		{
			this.name = name;
			_ifa_flags = ifa_flags;
			addresses = new List<IPAddress>();
		}

		internal void AddAddress(IPAddress address)
		{
			addresses.Add (address);
		}

		public override IPInterfaceProperties GetIPProperties ()
		{
			if (ipproperties == null)
			{
				ipproperties = new NuttxIPInterfaceProperties(this, addresses);
			}
			return ipproperties;
		}

		// public override IPv4InterfaceStatistics GetIPv4Statistics ()
		// {
		// 	if (ipv4stats == null)
		// 		ipv4stats = new NuttxIPv4InterfaceStatistics (this);
		// 	return ipv4stats;
		// }

		public override OperationalStatus OperationalStatus
		{
			get
			{
				if (((NuttxInterfaceFlags)_ifa_flags & NuttxInterfaceFlags.IFF_UP) == NuttxInterfaceFlags.IFF_UP)
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
			if (macAddress != null)
			{
				return new PhysicalAddress(macAddress);
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

			foreach (IPAddress address in addresses)
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
			get { return name; }
		}

		public override string Id
		{
			get { return name; }
		}

		public override bool IsReceiveOnly
		{
			get { return false; }
		}

		public override string Name
		{
			get { return name; }
		}

		public override NetworkInterfaceType NetworkInterfaceType
		{
			get { return type; }
		}

		public override long Speed
		{
			get { return 1000000; }			// Bits/s
		}

		internal int NameIndex
		{
			get { return UnixNetworkInterfaceAPI.if_nametoindex (Name); }
		}
	}
}
