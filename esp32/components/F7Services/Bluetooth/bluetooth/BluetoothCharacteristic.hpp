#ifndef _BLUETOOTH_CHARACTERISTIC_HPP_
#define _BLUETOOTH_CHARACTERISTIC_HPP_

#include <string>
#include <vector>
#include <functional>

class BluetoothService;
class BluetoothCharacteristic;
class BluetoothCharacteristicValue;

#include "BluetoothDescriptor.hpp"
#include "BluetoothCharacteristicDeclaration.hpp"
#include "BluetoothCharacteristicValue.hpp"

using namespace std;

typedef std::function<void(BluetoothService *)> ServiceAdded;
typedef std::function<void(BluetoothCharacteristic *)> CharacteristicAdded;
typedef std::function<void(BluetoothCharacteristic *)> ValueChanged;
typedef std::function<void(BluetoothDescriptor *)> DescriptorAdded;

class BluetoothCharacteristic
{
    public:
        BluetoothService *ParentService;

        BluetoothCharacteristic(Uuid16 uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength);
        BluetoothCharacteristic(Uuid128 uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength);

        BluetoothCharacteristicDeclaration *Declaration;
        BluetoothCharacteristicValue * Value;

        void AddDescriptor(BluetoothDescriptor *descriptor);

        ValueChanged OnValueChanged = NULL;
        DescriptorAdded OnDescriptorAdded = NULL;

        vector<BluetoothDescriptor*> GetDescriptorDefinitions() { return _descriptors; }
    
    private:
        vector<BluetoothDescriptor*> _descriptors;
};


#endif