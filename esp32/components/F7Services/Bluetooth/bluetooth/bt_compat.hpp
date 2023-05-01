#ifndef _BLUETOOTH_COMPAT_HPP_
#define _BLUETOOTH_COMPAT_HPP_

// compatibiltiy stuff - let it compile on desktop, etc while keeping ESP flavoring
#ifndef uint8_t
typedef unsigned char uint8_t;
#endif
#ifndef uint16_t
typedef unsigned short uint16_t;
#endif

#endif