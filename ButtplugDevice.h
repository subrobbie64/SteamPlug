#pragma once
#include "ButtplugDiscovery.h"

typedef void (*BatteryCallback)(void *arg, int batteryLevel);

enum BPStatus {
	BP_CONNECTING,
	BP_CONNECTED,
	BP_DISCONNECTED,
};

class ButtplugDevice {
public:
	ButtplugDevice(BtAddress address, const ButtplugDeviceDefinition* definition);
	void setBatteryCallback(BatteryCallback callback, void* arg);

	void connect();
	void waitForConnection();
	BPStatus checkConnectionStatus();

	void setVibrate(int percentage);
	bool readBatteryLevel();
private:
	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
		const unsigned char* const Value, const unsigned long Length);

	bool issueCommand(const char* commandString);

	CwclBluetoothManager _wclBluetoothManager;
	CwclGattClient _wclGattClient;

	BPStatus _status;

	wclGattService _buttplugService;
	wclGattCharacteristic _txCharac, _rxCharac;

	BatteryCallback _batteryCallback;
	void* _batteryCallbackArg;

	sysevent_t _connectedEvent;
	syssema_t _runningCommand;

	const ButtplugDeviceDefinition* _definition;
};