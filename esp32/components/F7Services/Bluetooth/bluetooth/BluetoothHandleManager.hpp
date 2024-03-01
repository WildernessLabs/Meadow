#ifndef _BLUETOOTH_HANDLE_MANAGER_HPP_
#define _BLUETOOTH_HANDLE_MANAGER_HPP_

#include "BluetoothService.hpp"
#include "BluetoothAttribute.hpp"
#include <atomic>

class BluetoothService;

class BluetoothHandleManager
{
    private:
        // NOTE: this value was arbitrarily decided based on an example.  No idea if it has meaning
        const unsigned short FIRST_HANDLE = 0x0021;

        std::atomic<unsigned short> _next_handle { FIRST_HANDLE };

        unsigned short GetHandle()
        {
            return _next_handle++;
        }

    public:
        BluetoothHandleManager() { }

        unsigned short Assign(BluetoothService *service);
        unsigned short Assign(BluetoothAttribute *attribute);
};

#endif