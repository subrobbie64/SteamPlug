#pragma once
#include "ButtplugDevice.h"

class HushDevice: public ButtplugDevice {
public:
	HushDevice(ButtplugConfig& config);
	virtual ~HushDevice();

	virtual void connect();
	virtual bool isConnected();
	virtual const std::string& getDeviceName() const;

	virtual void setVibrate(unsigned char effectiveVibrationPercent);
	virtual int getBatteryLevel() const;
	bool readBatteryLevel();

	const std::string& getDeviceId() const;

	static const ButtplugDeviceDefinition HUSH_DEVICE[];
	static const int NUM_HUSH_DEVICES;
private:
	enum Status {
		BP_DISCONNECTED,
		BP_CONNECTING,
		BP_CONNECTED
	};

	void disconnect();

	std::string getGapName();

	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
		const unsigned char* const Value, const unsigned long Length);

	bool issueCommand(const char* commandString);

	CwclBluetoothManager _wclBluetoothManager;
	CwclGattClient _wclGattClient;

	Status _status;
	std::string _deviceName;
	std::string _deviceId;

	wclGattService _buttplugService;
	wclGattCharacteristic _txCharac, _rxCharac;

	sysevent_t _connectedEvent;
	syssema_t _runningCommand;

	int _vibration;

	int _batteryLevel;

	unsigned long long _connectRetryAt;

	const ButtplugDeviceDefinition* _definition;

	static const int MAX_VIBRATION_SETTING;
	static const int CONNECT_RETRY_MS;

	static const wclGattUuid GENERIC_ACCESS_SERVICE_UUID, DEVICE_NAME_CHARAC_UUID;
};

