#include "BluetoothCharacteristicValue.hpp"

BluetoothCharacteristicValue::BluetoothCharacteristicValue(BluetoothCharacteristic *parent, Uuid16 uuid, CharacteristicPermission permission, int maxValueLength)
{
    _parent = parent;
    uuid16 = uuid;
    Permission = permission;
    _value = new uint8_t[maxValueLength];
    MaxLength = maxValueLength;
}

BluetoothCharacteristicValue::BluetoothCharacteristicValue(BluetoothCharacteristic *parent, Uuid128 uuid, CharacteristicPermission permission, int maxValueLength)
{
    _parent = parent;
    uuid128 = uuid;
    Permission = permission;
    _value = new uint8_t[maxValueLength];
    MaxLength = maxValueLength;
    _is128 = true;
}

BluetoothCharacteristicValue::~BluetoothCharacteristicValue()
{
    if(_value)
    {
        delete _value;
        _value = NULL;
    }
}

bool BluetoothCharacteristicValue::WriteValue(uint8_t* source, int length)
{
    if(length > MaxLength) return false;
    if(length < 0) return false;

    if(length > 0)
    {
        memcpy(_value, source, length);

        if(_parent->OnValueChanged)
        {
            _parent->OnValueChanged(this->_parent);
        }
    }

    return true;
}

bool BluetoothCharacteristicValue::ReadValue(uint8_t* dest, int length)
{
    if(length > CurrentLength) return false;
    if(length < 0) return false;

    if(length > 0)
    {
        memcpy(dest, _value, length);
    }

    return true;
}

