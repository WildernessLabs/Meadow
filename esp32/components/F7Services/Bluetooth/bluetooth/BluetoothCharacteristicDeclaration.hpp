#ifndef _BLUETOOTH_CHARACTERISTIC_DECLARATION_HPP_
#define _BLUETOOTH_CHARACTERISTIC_DECLARATION_HPP_

#include <string>
#include <vector>

#include "BluetoothAttribute.hpp"
#include "BluetoothCharacteristic.hpp"

using namespace std;

class BluetoothCharacteristicDeclaration : public BluetoothAttribute
{
    private:
        bool _is128 = false;

    public:
        BluetoothCharacteristicDeclaration(Uuid16 value_uuid, CharacteristicPermission permission, CharacteristicProperty properties) 
        {
            Uuid = Uuid16::CharacteristicDeclaration;
            Permission = permission;
            Properties = properties;
            ValueUuid16 = value_uuid;
        }

        BluetoothCharacteristicDeclaration(Uuid128 value_uuid, CharacteristicPermission permission, CharacteristicProperty properties) 
        {
            Uuid = Uuid16::CharacteristicDeclaration;
            Permission = permission;
            Properties = properties;
            ValueUuid128 = value_uuid;
            _is128 = true;
        }

        Uuid16 Uuid;
        CharacteristicProperty Properties; // A bitfield listing the permitted operations on this characteristic
        uint16_t ValueHandle; // The handle of the attribute containing the characteristic value

        bool Is128() { return _is128; }

        Uuid16 ValueUuid16;
        Uuid128 ValueUuid128;
};

#endif