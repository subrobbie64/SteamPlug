#pragma once

#include "ButtplugDevice.h"
#include <deque>

class ButtplugConfig;

class ButtplugDiscovery {
public:
	ButtplugDiscovery(const std::string& deviceName);
	virtual ~ButtplugDiscovery();

	bool runDiscovery(ButtplugConfig* config);
	BtAddress getDiscoveredDevice() const;

protected:
	virtual bool probeDevice(const std::string& gapName, BtAddress address) = 0;
	virtual void storeAdditionalAttributes(ButtplugConfig* config) = 0;

	CwclGattClient _wclGattClient;
private:
	CwclBluetoothRadio* getRadio();
	void wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address);

	void wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio);
	void wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error);

	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void inspectNextDevice();

	const std::string _deviceName;

	CwclBluetoothManager _wclBluetoothManager;

	std::deque<BtAddress> _discoveredDevices;
	BtAddress _discoveredIntendedDevice;
	sysevent_t _discoveryCompletedEvent;
};
