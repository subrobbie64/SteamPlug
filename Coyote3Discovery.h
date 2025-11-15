#pragma once
#include "Coyote3Device.h"
#include "System.h"
#include "ButtplugDiscovery.h"
#include <deque>

using namespace wclBluetooth;

class CoyoteDiscovery : public ButtplugDiscovery {
public:
	CoyoteDiscovery();
	~CoyoteDiscovery();

	virtual bool runDiscovery(ButtplugConfig* config);
private:
	CwclBluetoothRadio* getRadio();
	void wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address);

	void wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio);
	void wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error);

	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void inspectNextDevice();

	CwclBluetoothManager _wclBluetoothManager;
	CwclGattClient _wclGattClient;

	BtAddress _discoveredCoyoteAddress;

	std::deque<BtAddress> _discoveredDevices;

	sysevent_t _discoveryCompletedEvent;

	static const tstring EXPECTED_DEVICE_NAME;
};

