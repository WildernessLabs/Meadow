using System.Runtime.InteropServices;

namespace System.Net.NetworkInformation
{
	namespace NuttxStructs
	{
		internal struct ifaddrs
		{
			public IntPtr  ifa_next;
			public string  ifa_name;
			public uint    ifa_flags;
			public IntPtr  ifa_addr;
			public IntPtr  ifa_netmask;
			public IntPtr  ifa_dstaddr;
			public IntPtr  ifa_data;
		}

		/// <summary>
		/// Mapping for struct sockaddr from sys/socket.h
		/// </summary>
		internal struct sockaddr
		{
			public byte  	sa_family;
			public IntPtr	sa_data;
		}

		/// <summary>
		/// Mapping for struct sockaddr_in from netinet/in.h.
		/// </summary>
		internal struct sockaddr_in
		{
			public ushort	sin_family;
			public ushort	sin_port;
			public uint		sin_addr;
		}

		/// <summary>
		/// Mapping for struct in6_addr from sys/socket.h.
		/// </summary>
		internal struct in6_addr
		{
			[MarshalAs (UnmanagedType.ByValArray, SizeConst=16)]
			public byte[] u6_addr8;
		}

		/// <summary>
		/// Mapping for struct sockaddr_in6 from sys/socket.h.
		/// </summary>
		internal struct sockaddr_in6
		{
			public ushort   sin6_family;
			public ushort   sin6_port;
			public uint     sin6_flowinfo;
			public in6_addr sin6_addr;
			public uint     sin6_scope_id;
		}
	}
	internal enum NuttxInterfaceFlags
	{
		IFF_DOWN = (1 << 0),        /* Interface is down. */
		IFF_UP = (1 << 1),          /* Interface is up. */
		IFF_RUNNING = (1 << 2),     /* Carrier is available. */
		IFF_IPV6 = (1 << 3),        /* Configured for IPv6 packets. */
		IFF_NOARP = (1 << 7)        /* AR{ not required for this packet. */
	}
}
