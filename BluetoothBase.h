#pragma once

#include <wclBluetooth.h>

using namespace wclBluetooth;

typedef unsigned long long BtAddress;

class BluetoothBase {
public:
	BluetoothBase();
	virtual ~BluetoothBase();

	static std::string MacToString(BtAddress Address);
	static BtAddress MacFromString(const char* str);
	static std::string UuidToString(wclGattUuid uuid);
protected:
	CwclBluetoothRadio* getRadio();
	std::string getDeviceName(BtAddress address);

	CwclBluetoothManager _wclBluetoothManager;
};
