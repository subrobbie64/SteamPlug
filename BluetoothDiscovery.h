#pragma once

#include "BluetoothBase.h"
#include "System.h"
#include <deque>

class ButtplugConfig;

class BluetoothDiscovery : public BluetoothBase {
public:
	BluetoothDiscovery(const std::string& deviceName);
	virtual ~BluetoothDiscovery();

	bool runDiscovery(ButtplugConfig* config);
	BtAddress getDiscoveredDevice() const;

protected:
	virtual bool probeDevice(BtAddress address, const std::string& gapName, wclGattServices& btServices) = 0;
	virtual void storeAdditionalAttributes(ButtplugConfig* config) = 0;

	CwclGattClient _wclGattClient;
private:
	void wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address);

	void wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio);
	void wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error);

	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void inspectNextDevice();

	const std::string _deviceName;

	std::deque<BtAddress> _discoveredDevices;
	BtAddress _discoveredIntendedDevice;
	sysevent_t _discoveryCompletedEvent;
};
