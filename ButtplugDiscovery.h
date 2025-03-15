#pragma once

#include <vector>
#include <wclBluetooth.h>
#include "System.h"

using namespace wclBluetooth;

struct ButtplugDeviceDefinition {
	wclGattUuid serviceId;
	wclGattUuid txCharacteristicId;
	wclGattUuid rxCharacteristicId;
};

typedef unsigned long long BtAddress;

class ButtplugConfig;

class ButtplugDiscovery {
public:
	ButtplugDiscovery();

	bool runDiscovery();
	
	ButtplugConfig* getAsConfiguration();

	static const ButtplugDeviceDefinition HUSH_DEVICE[];
	static const int NUM_HUSH_DEVICES;
private:
	void wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio,
		const __int64 Address);

	void wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio);
	void wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio,
		const int Error);
	
	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void inspectNextDevice();

	int getHushDeviceType(BtAddress address, wclGattServices& services);

	CwclBluetoothManager _wclBluetoothManager;
	CwclGattClient _wclGattClient;
	
	std::vector<BtAddress> _discoveredDevices;

	int _inspectedDeviceNumber;

	sysevent_t _discoveryCompletedEvent;

	BtAddress _discoveredHushDevice;
	int _discoveredHushDeviceType;
};