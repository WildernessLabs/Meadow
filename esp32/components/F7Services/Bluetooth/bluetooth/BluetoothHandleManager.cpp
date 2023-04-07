#include "BluetoothHandleManager.hpp"


unsigned short BluetoothHandleManager::Assign(BluetoothService *service)
{
    auto h = GetHandle();
    service->Handle = h;
    return h;

}

unsigned short BluetoothHandleManager::Assign(BluetoothAttribute *attribute)
{
    auto h = GetHandle();
    attribute->Handle = h;
    return h;
}
