// GENERATED FILE, DO NOT MODIFY

extern int SystemNative_ConvertErrorPlatformToPal (int);
extern int SystemNative_ConvertErrorPalToPlatform (int);
extern int SystemNative_StrErrorR (int,int,int);
extern void SystemNative_GetNonCryptographicallySecureRandomBytes (int,int);
extern int SystemNative_OpenDir (int);
extern int SystemNative_GetReadDirRBufferSize (void);
extern int SystemNative_ReadDirR (int,int,int,int);
extern int SystemNative_CloseDir (int);
extern int SystemNative_ReadLink (int,int,int);
extern int SystemNative_ChMod (int,int);
extern int SystemNative_CopyFile (int,int);
extern int SystemNative_LChflags (int,int);
extern int SystemNative_LChflagsCanSetHiddenFlag (void);
extern int SystemNative_Link (int,int);
extern int SystemNative_MkDir (int,int);
extern int SystemNative_Rename (int,int);
extern int SystemNative_RmDir (int);
extern int SystemNative_Stat2 (int,int);
extern int SystemNative_LStat2 (int,int);
extern int SystemNative_Unlink (int);

#if !defined(ENABLE_NETCORE)
extern int SystemNative_FStat2 (int,int);
extern int SystemNative_Stat2 (int,int);
extern int SystemNative_LStat2 (int,int);
extern int SystemNative_Symlink (int,int);
extern int SystemNative_GetEGid (void);
extern int SystemNative_GetEUid (void);
extern int SystemNative_UTime (int,int);
extern int SystemNative_UTimes (int,int);
#endif

struct HostEntry;
int32_t SystemNative_GetHostEntryForName(const uint8_t* address, struct HostEntry* entry);
struct IPAddress;
struct addrinfo;
int32_t SystemNative_GetNextIPAddress(const struct HostEntry* entry, struct addrinfo** addressListHandle, struct IPAddress* endPoint);
void SystemNative_FreeHostEntry(struct HostEntry* entry);
int32_t SystemNative_GetNameInfo(const uint8_t* address,
                               int32_t addressLength,
                               int8_t isIPv6,
                               uint8_t* host,
                               int32_t hostLength,
                               uint8_t* service,
                               int32_t serviceLength,
                               int32_t flags);
int32_t SystemNative_GetDomainName(uint8_t* name, int32_t nameLength);
int32_t SystemNative_GetHostName(uint8_t* name, int32_t nameLength);
int32_t SystemNative_GetIPSocketAddressSizes(int32_t* ipv4SocketAddressSize, int32_t* ipv6SocketAddressSize);
int32_t SystemNative_GetAddressFamily(const uint8_t* socketAddress, int32_t socketAddressLen, int32_t* addressFamily);
int32_t SystemNative_SetAddressFamily(uint8_t* socketAddress, int32_t socketAddressLen, int32_t addressFamily);
int32_t SystemNative_GetPort(const uint8_t* socketAddress, int32_t socketAddressLen, uint16_t* port);
int32_t SystemNative_SetPort(uint8_t* socketAddress, int32_t socketAddressLen, uint16_t port);
int32_t SystemNative_GetIPv4Address(const uint8_t* socketAddress, int32_t socketAddressLen, uint32_t* address);
int32_t SystemNative_SetIPv4Address(uint8_t* socketAddress, int32_t socketAddressLen, uint32_t address);
int32_t SystemNative_GetIPv6Address(
    const uint8_t* socketAddress, int32_t socketAddressLen, uint8_t* address, int32_t addressLen, uint32_t* scopeId);
int32_t SystemNative_SetIPv6Address(
    uint8_t* socketAddress, int32_t socketAddressLen, uint8_t* address, int32_t addressLen, uint32_t scopeId);
int32_t SystemNative_GetControlMessageBufferSize(int32_t isIPv4, int32_t isIPv6);
struct MessageHeader;
struct IPPacketInformation;
int32_t SystemNative_TryGetIPPacketInformation(
    struct MessageHeader* messageHeader, int32_t isIPv4, struct IPPacketInformation* packetInfo);
struct IPv4MulticastOption;
int32_t SystemNative_GetIPv4MulticastOption(intptr_t socket, int32_t multicastOption, struct IPv4MulticastOption* option);
int32_t SystemNative_SetIPv4MulticastOption(intptr_t socket, int32_t multicastOption, struct IPv4MulticastOption* option);
struct IPv6MulticastOption;
int32_t SystemNative_GetIPv6MulticastOption(intptr_t socket, int32_t multicastOption, struct IPv6MulticastOption* option);
int32_t SystemNative_SetIPv6MulticastOption(intptr_t socket, int32_t multicastOption, struct IPv6MulticastOption* option);
struct LingerOption;
int32_t SystemNative_GetLingerOption(intptr_t socket, struct LingerOption* option);
int32_t SystemNative_SetLingerOption(intptr_t socket, struct LingerOption* option);
int32_t SystemNative_SetReceiveTimeout(intptr_t socket, int32_t millisecondsTimeout);
int32_t SystemNative_SetSendTimeout(intptr_t socket, int32_t millisecondsTimeout);
int32_t SystemNative_ReceiveMessage(intptr_t socket, struct MessageHeader* messageHeader, int32_t flags, int64_t* received);
int32_t SystemNative_SendMessage(intptr_t socket, struct MessageHeader* messageHeader, int32_t flags, int64_t* sent);
int32_t SystemNative_Accept(intptr_t socket, uint8_t* socketAddress, int32_t* socketAddressLen, intptr_t* acceptedSocket);
int32_t SystemNative_Bind(intptr_t socket, int32_t protocolType, uint8_t* socketAddress, int32_t socketAddressLen);
int32_t SystemNative_Connect(intptr_t socket, uint8_t* socketAddress, int32_t socketAddressLen);
int32_t SystemNative_GetPeerName(intptr_t socket, uint8_t* socketAddress, int32_t* socketAddressLen);
int32_t SystemNative_GetSockName(intptr_t socket, uint8_t* socketAddress, int32_t* socketAddressLen);
int32_t SystemNative_Listen(intptr_t socket, int32_t backlog);
int32_t SystemNative_Shutdown(intptr_t socket, int32_t socketShutdown);
int32_t SystemNative_GetSocketErrorOption(intptr_t socket, int32_t* error);
int32_t SystemNative_GetSockOpt(
    intptr_t socket, int32_t socketOptionLevel, int32_t socketOptionName, uint8_t* optionValue, int32_t* optionLen);
int32_t SystemNative_SetSockOpt(
    intptr_t socket, int32_t socketOptionLevel, int32_t socketOptionName, uint8_t* optionValue, int32_t optionLen);
int32_t SystemNative_Socket(int32_t addressFamily, int32_t socketType, int32_t protocolType, intptr_t* createdSocket);
int32_t SystemNative_GetAtOutOfBandMark(intptr_t socket, int32_t* available);
int32_t SystemNative_GetBytesAvailable(intptr_t socket, int32_t* available);
int32_t SystemNative_CreateSocketEventPort(intptr_t* port);
int32_t SystemNative_CloseSocketEventPort(intptr_t port);
    struct SocketEvent;
int32_t SystemNative_CreateSocketEventBuffer(int32_t count, struct SocketEvent** buffer);
int32_t SystemNative_FreeSocketEventBuffer(struct SocketEvent* buffer);
int32_t SystemNative_TryChangeSocketEventRegistration(
    intptr_t port, intptr_t socket, int32_t currentEvents, int32_t newEvents, uintptr_t data);
int32_t SystemNative_WaitForSocketEvents(intptr_t port, struct SocketEvent* buffer, int32_t* count);
int32_t SystemNative_PlatformSupportsDualModeIPv4PacketInfo(void);
char* SystemNative_GetPeerUserName(intptr_t socket);
void SystemNative_GetDomainSocketSizes(int32_t* pathOffset, int32_t* pathSize, int32_t* addressSize);
int32_t SystemNative_SendFile(intptr_t out_fd, intptr_t in_fd, int64_t offset, int64_t count, int64_t* sent);

static MonoDlMapping system_native_mappings[] = {
    {"SystemNative_ConvertErrorPlatformToPal", SystemNative_ConvertErrorPlatformToPal},
    {"SystemNative_ConvertErrorPalToPlatform", SystemNative_ConvertErrorPalToPlatform},
    {"SystemNative_StrErrorR", SystemNative_StrErrorR},
    {"SystemNative_GetNonCryptographicallySecureRandomBytes", SystemNative_GetNonCryptographicallySecureRandomBytes},
    {"SystemNative_OpenDir", SystemNative_OpenDir},
    {"SystemNative_GetReadDirRBufferSize", SystemNative_GetReadDirRBufferSize},
    {"SystemNative_ReadDirR", SystemNative_ReadDirR},
    {"SystemNative_CloseDir", SystemNative_CloseDir},
    {"SystemNative_ReadLink", SystemNative_ReadLink},
    {"SystemNative_ChMod", SystemNative_ChMod},
    {"SystemNative_CopyFile", SystemNative_CopyFile},
    {"SystemNative_LChflags", SystemNative_LChflags},
    {"SystemNative_LChflagsCanSetHiddenFlag", SystemNative_LChflagsCanSetHiddenFlag},
    {"SystemNative_Link", SystemNative_Link},
    {"SystemNative_MkDir", SystemNative_MkDir},
    {"SystemNative_Rename", SystemNative_Rename},
    {"SystemNative_RmDir", SystemNative_RmDir},
    {"SystemNative_Unlink", SystemNative_Unlink},
#if !defined(ENABLE_NETCORE)
    {"SystemNative_FStat2", SystemNative_FStat2},
    {"SystemNative_Stat2", SystemNative_Stat2},
    {"SystemNative_LStat2", SystemNative_LStat2},
    {"SystemNative_Symlink", SystemNative_Symlink},
    {"SystemNative_GetEGid", SystemNative_GetEGid},
    {"SystemNative_GetEUid", SystemNative_GetEUid},
    {"SystemNative_UTime", SystemNative_UTime},
    {"SystemNative_UTimes", SystemNative_UTimes},
#endif

    {"SystemNative_GetHostEntryForName", SystemNative_GetHostEntryForName},
    {"SystemNative_GetNextIPAddress", SystemNative_GetNextIPAddress},
    {"SystemNative_FreeHostEntry", SystemNative_FreeHostEntry},
    {"SystemNative_GetNameInfo", SystemNative_GetNameInfo},
    {"SystemNative_GetDomainName", SystemNative_GetDomainName},
    {"SystemNative_GetHostName", SystemNative_GetHostName},
    {"SystemNative_GetIPSocketAddressSizes", SystemNative_GetIPSocketAddressSizes},
    {"SystemNative_GetAddressFamily", SystemNative_GetAddressFamily},
    {"SystemNative_SetAddressFamily", SystemNative_SetAddressFamily},
    {"SystemNative_GetPort", SystemNative_GetPort},
    {"SystemNative_SetPort", SystemNative_SetPort},
    {"SystemNative_GetIPv4Address", SystemNative_GetIPv4Address},
    {"SystemNative_SetIPv4Address", SystemNative_SetIPv4Address},
    {"SystemNative_GetIPv6Address", SystemNative_GetIPv6Address},
    {"SystemNative_SetIPv6Address", SystemNative_SetIPv6Address},
    {"SystemNative_GetControlMessageBufferSize", SystemNative_GetControlMessageBufferSize},
    {"SystemNative_TryGetIPPacketInformation", SystemNative_TryGetIPPacketInformation},
    {"SystemNative_GetIPv4MulticastOption", SystemNative_GetIPv4MulticastOption},
    {"SystemNative_SetIPv4MulticastOption", SystemNative_SetIPv4MulticastOption},
    {"SystemNative_GetIPv6MulticastOption", SystemNative_GetIPv6MulticastOption},
    {"SystemNative_SetIPv6MulticastOption", SystemNative_SetIPv6MulticastOption},
    {"SystemNative_GetLingerOption", SystemNative_GetLingerOption},
    {"SystemNative_SetLingerOption", SystemNative_SetLingerOption},
    {"SystemNative_SetReceiveTimeout", SystemNative_SetReceiveTimeout},
    {"SystemNative_SetSendTimeout", SystemNative_SetSendTimeout},
    {"SystemNative_ReceiveMessage", SystemNative_ReceiveMessage},
    {"SystemNative_SendMessage", SystemNative_SendMessage},
    {"SystemNative_Accept", SystemNative_Accept},
    {"SystemNative_Bind", SystemNative_Bind},
    {"SystemNative_Connect", SystemNative_Connect},
    {"SystemNative_GetPeerName", SystemNative_GetPeerName},
    {"SystemNative_GetSockName", SystemNative_GetSockName},
    {"SystemNative_Listen", SystemNative_Listen},
    {"SystemNative_Shutdown", SystemNative_Shutdown},
    {"SystemNative_GetSocketErrorOption", SystemNative_GetSocketErrorOption},
    {"SystemNative_GetSockOpt", SystemNative_GetSockOpt},
    {"SystemNative_SetSockOpt", SystemNative_SetSockOpt},
    {"SystemNative_Socket", SystemNative_Socket},
    {"SystemNative_GetAtOutOfBandMark", SystemNative_GetAtOutOfBandMark},
    {"SystemNative_GetBytesAvailable", SystemNative_GetBytesAvailable},
    {"SystemNative_CreateSocketEventPort", SystemNative_CreateSocketEventPort},
    {"SystemNative_CloseSocketEventPort", SystemNative_CloseSocketEventPort},
    {"SystemNative_CreateSocketEventBuffer", SystemNative_CreateSocketEventBuffer},
    {"SystemNative_FreeSocketEventBuffer", SystemNative_FreeSocketEventBuffer},
    {"SystemNative_TryChangeSocketEventRegistration", SystemNative_TryChangeSocketEventRegistration},
    {"SystemNative_WaitForSocketEvents", SystemNative_WaitForSocketEvents},
    {"SystemNative_PlatformSupportsDualModeIPv4PacketInfo", SystemNative_PlatformSupportsDualModeIPv4PacketInfo},
    {"SystemNative_GetPeerUserName", SystemNative_GetPeerUserName},
    {"SystemNative_GetDomainSocketSizes", SystemNative_GetDomainSocketSizes},
    {"SystemNative_SendFile", SystemNative_SendFile},

    {NULL, NULL}
};

