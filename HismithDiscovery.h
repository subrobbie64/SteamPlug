#pragma once
#include "ButtplugDevice.h"
#include "ButtplugDiscovery.h"
#include "System.h"
#include <deque>

using namespace wclBluetooth;

class HismithDiscovery : public ButtplugDiscovery {
public:
	HismithDiscovery();
	~HismithDiscovery();

	virtual bool runDiscovery(ButtplugConfig* config);
private:
	CwclBluetoothRadio* getRadio();
	void wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address);

	void wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio);
	void wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error);

	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void inspectNextDevice();

	bool isHismithDevice(BtAddress address, wclGattServices& services);

	CwclBluetoothManager _wclBluetoothManager;
	CwclGattClient _wclGattClient;

	std::deque<BtAddress> _discoveredDevices;

	BtAddress _discoveredHismithDevice;

	sysevent_t _discoveryCompletedEvent;
};