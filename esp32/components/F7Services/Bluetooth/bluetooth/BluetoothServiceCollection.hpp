#ifndef _BLUETOOTH_SERVICE_COLLECTION_HPP_
#define _BLUETOOTH_SERVICE_COLLECTION_HPP_

#include <string>
#include <vector>
#include <functional>

#include "Uuid.hpp"
#include "BluetoothService.hpp"
#include "BluetoothCharacteristic.hpp"
#include "BluetoothHandleManager.hpp"

using namespace std;

class BluetoothServiceCollection
{
    private:
        vector<BluetoothService*> _services;
        BluetoothHandleManager* _handleManager;

        void HandleServiceCharacteristicAdded(BluetoothCharacteristic *);
        void HandleServiceValueChanged(BluetoothCharacteristic *characteristic);

    public:
        BluetoothServiceCollection(BluetoothHandleManager *handleManager)
        {
            _handleManager = handleManager;
        }

        ServiceAdded OnServiceAdded = NULL;
        CharacteristicAdded OnCharacteristicAdded = NULL;
        ValueChanged OnValueChanged = NULL;

        BluetoothService& Add(BluetoothService *service);
        BluetoothService* Find(uint8_t *uuid);
        BluetoothService* Find(Uuid16 uuid);
        int Count();

        vector<BluetoothService*> GetServiceDefinitions();
};

#endif