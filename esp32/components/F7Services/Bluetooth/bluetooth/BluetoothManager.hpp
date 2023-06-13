#ifndef _BLUETOOTH_MANAGER_HPP_
#define _BLUETOOTH_MANAGER_HPP_

#include "BluetoothServiceCollection.hpp"
#include "BluetoothAttribute.hpp"
#include "BluetoothHandleManager.hpp"
#include <vector>
#include <map>

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"


using namespace std;

class BluetoothManager 
{
    private:
        struct length_val
        {
            uint16_t length;
            uint8_t* value;
        };

        BluetoothHandleManager _handleManager;
        std::map<uint16_t, length_val> _valueMap;

        static void HandleServiceAdded(BluetoothService *);
        static void HandleCharacteristicAdded(BluetoothCharacteristic *);
        static void HandleValueChanged(BluetoothCharacteristic *);
        static void HandleDescriptorAdded(BluetoothDescriptor *);

        esp_gatts_attr_db_t* _attribute_table;
        uint16_t _attributeTableLength;
        esp_ble_adv_data_t _advertisingData;
        esp_ble_adv_params_t _advertisingParameters;
        esp_ble_adv_data_t _scanResponseData;
        
    public:
        char DeviceName[256];

        BluetoothManager();
        BluetoothServiceCollection Services;


        void GenerateAttributeTable();
        void GenerateAdvertisingData();
        void SetHandles(uint16_t* handles, uint16_t count);
        int GetHandleCount() { return _attributeTableLength; }
        void GetHandles(uint16_t* handles);

        esp_gatts_attr_db_t* GetAttributeTable();
        esp_ble_adv_data_t *GetAdvertisingData();
        esp_ble_adv_data_t *GetScanResponseData();
        esp_ble_adv_params_t *GetAdvertisingParameters();
        uint16_t GetAttributeTableLength();

        void GetCurrentValueForAttributeHandle(uint16_t handle, uint8_t* pData, uint16_t* pLen);
        void SetCurrentValueForAttributeHandle(uint16_t handle, uint8_t* pData, uint16_t len);

        void BuildGraphFromJson(char *cfg_json);


};

#endif