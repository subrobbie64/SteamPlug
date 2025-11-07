#pragma once
#include "ButtplugDevice.h"
#include "System.h"
#include <deque>

using namespace wclBluetooth;

class ButtplugConfig;

class ButtplugDiscovery {
public:
	virtual ButtplugConfig* runDiscovery() = 0;
};

class Hush2ButtplugDiscovery : public ButtplugDiscovery {
public:
	Hush2ButtplugDiscovery();
	~Hush2ButtplugDiscovery();

	virtual ButtplugConfig* runDiscovery();
private:
	CwclBluetoothRadio* getRadio();
	void wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address);

	void wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio);
	void wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error);
	
	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void inspectNextDevice();

	int getHushDeviceType(BtAddress address, wclGattServices& services);

	CwclBluetoothManager _wclBluetoothManager;
	CwclGattClient _wclGattClient;
	
	std::deque<BtAddress> _discoveredDevices;

	BtAddress _discoveredHushDevice;
	int _discoveredHushDeviceType;
	std::string _discoveredHushName;

	sysevent_t _discoveryCompletedEvent;
};