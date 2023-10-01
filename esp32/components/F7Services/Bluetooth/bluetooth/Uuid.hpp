#ifndef _BLUETOOTH_UUID_HPP_
#define _BLUETOOTH_UUID_HPP_

#include "string.h"
#include "bt_compat.hpp"

#include <string>

/*
128-bit UUID (GUID)
*/
struct Uuid128
{
    uint8_t data[16];

    Uuid128(const char *);
    Uuid128();
    Uuid128(uint8_t *other);
    Uuid128(const Uuid128&) = default;
    
    std::string ToString() const;

    bool operator==(const Uuid128  &other) const
    {
        return memcmp(data, other.data, 16) == 0;
    }

    Uuid128 operator=(const Uuid128  &other) const
    {
        memcpy((void*)data, other.data, 16);
        return other;
    }

    uint8_t* operator=(uint8_t *other) const
    {
        memcpy((void*)data, other, 16);
        return other;
    }
};

/*
16-bit UUID
*/
struct Uuid16
{
    uint8_t data[2];

    operator uint16_t() const 
    { 
        return (uint16_t)*data; 
    }

    Uuid16();
    Uuid16(uint16_t value);
    Uuid16(uint8_t *other);
    Uuid16(const Uuid16&) = default;

    bool operator==(const Uuid16  &other) const
    {
        return memcmp(data, other.data, 2) == 0;
    }

    Uuid16 operator=(const Uuid16  &other) const
    {
        memcpy((void*)data, other.data, 2);
        return other;
    }

    uint8_t* operator=(uint8_t *other) const
    {
        memcpy((void*)data, other, 2);
        return other;
    }

    static Uuid16 PrimaryService;
    static Uuid16 CharacteristicDeclaration;
};

#endif
