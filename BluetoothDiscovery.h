#pragma once

#include "BluetoothBase.h"
#include "System.h"
#include <deque>

class ButtplugConfig;

enum BtDeviceType {
	BTD_HUSH2,
	BTD_COYOTE3,
	BTD_HISMITH
};

class BluetoothDiscovery : public BluetoothBase {
public:
	BluetoothDiscovery(BtDeviceType devType, const std::string& deviceName);
	virtual ~BluetoothDiscovery();

	bool runDiscovery(ButtplugConfig* config);
	BtAddress getDiscoveredDevice() const;

protected:
	bool probeDevice(BtAddress address, const std::string& gapName, wclGattServices& btServices);

	CwclGattClient _wclGattClient;
private:
	void wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address);

	void wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio);
	void wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error);

	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void inspectNextDevice();

	const BtDeviceType _deviceType;
	const std::string _deviceName;

	std::deque<BtAddress> _discoveredDevices;
	BtAddress _discoveredIntendedDevice;
	sysevent_t _discoveryCompletedEvent;
};
