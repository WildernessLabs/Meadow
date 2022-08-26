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

		internal struct macaddress
		{
			public ushort   sa_family;

			[MarshalAs(UnmanagedType.ByValArray, SizeConst=6)]
			public byte[] 	address;
		}

		/// <summary>
		/// Mapping for struct in6_addr from sys/socket.h.
		/// </summary>
		internal struct in6_addr
		{
			[MarshalAs(UnmanagedType.ByValArray, SizeConst=16)]
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

		internal struct NuttxInterfaceFlags
		{

			public const uint IFF_DOWN = (1 << 0);        	/* Interface is down. */
			public const uint IFF_UP = (1 << 1);          	/* Interface is up. */
			public const uint IFF_RUNNING = (1 << 2);     	/* Carrier is available. */
			public const uint IFF_IPV6 = (1 << 3);        	/* Configured for IPv6 packets. */
			public const uint IFF_NOARP = (1 << 7);       	/* ARP not required for this packet. */
			public const uint IFF_WIFI = (1 << 6);		/* Indicate that this is a WiFi interface. */
		}
	}
}
