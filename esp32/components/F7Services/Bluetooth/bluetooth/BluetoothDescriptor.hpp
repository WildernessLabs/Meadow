#ifndef _BLUETOOTH_DESCRIPTOR_HPP_
#define _BLUETOOTH_DESCRIPTOR_HPP_

#include "BluetoothAttribute.hpp"
#include "Uuid.hpp"

class BluetoothDescriptor : public BluetoothAttribute
{
    private:
        BluetoothCharacteristic *_parent;
        bool _is128 = false;

    public: 
        BluetoothDescriptor(BluetoothCharacteristic *parent, Uuid128 uuid, CharacteristicPermission permission, uint16_t maxValueLength)
        {
            _parent = parent;
            uuid128 = uuid;
            Permission = permission;
            MaxLength = maxValueLength;
            Value = new uint8_t[MaxLength];
            memset(Value, 0, MaxLength);
            _is128 = true;
        }

        BluetoothDescriptor(BluetoothCharacteristic *parent, Uuid16 uuid, CharacteristicPermission permission, uint16_t maxValueLength)
        {
            _parent = parent;
            uuid16 = uuid;
            Permission = permission;
            MaxLength = maxValueLength;
            Value = new uint8_t[MaxLength];
            memset(Value, 0, MaxLength);
        }

        void SetValue(int value)
        {
            if(sizeof(int) > MaxLength)
            {
                return;
            }
            memset(Value, 0, MaxLength);
            memcpy(Value, &value, sizeof(int));
        }

        std::string Name;

        bool Is128() { return _is128; }

        Uuid16 uuid16;
        Uuid128 uuid128;

        uint16_t MaxLength;
        uint16_t CurrentLength;
        uint8_t *Value;

};


#endif