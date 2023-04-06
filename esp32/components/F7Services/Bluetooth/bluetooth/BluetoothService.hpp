#ifndef _BLUETOOTH_SERVICE_HPP_
#define _BLUETOOTH_SERVICE_HPP_

#include <string>
#include <vector>

#include "Uuid.hpp"
#include "BluetoothCharacteristic.hpp"
#include "BluetoothHandleManager.hpp"

using namespace std;

class BluetoothHandleManager;

class BluetoothService
{
    public:
        BluetoothService();
        BluetoothService(const char* name, Uuid16 uuid);
        BluetoothCharacteristic& AddCharacteristic(BluetoothCharacteristic *characteristic);

        CharacteristicAdded OnCharacteristicAdded = NULL;

        std::string Name;
        Uuid16 Uuid;
        unsigned short Handle;

        void SetHandleManager(BluetoothHandleManager *handleManager) { _handleManager = handleManager; }

        vector<BluetoothCharacteristic*> GetCharacteristicDefinitions() { return _characteristics; }

    private:
        vector<BluetoothCharacteristic*> _characteristics;
        BluetoothHandleManager* _handleManager;
};

#endif