#pragma once

#include "ButtplugDevice.h"

class HismithDevice : public ButtplugDevice {
public:
	HismithDevice(ButtplugConfig& config);
	virtual ~HismithDevice();

	virtual void connect();
	virtual bool isConnected();
	virtual const std::string& getDeviceName() const;

	virtual void setVibrate(unsigned char effectiveVibrationPercent);
	virtual int getBatteryLevel() const;
private:
	enum Status {
		BP_DISCONNECTED,
		BP_CONNECTING,
		BP_CONNECTED
	};

	void setFuckMachineSpeed(int speed);
	void disconnect();

	std::string getGapName();

	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
		const unsigned char* const Value, const unsigned long Length);

	CwclBluetoothManager _wclBluetoothManager;
	CwclGattClient _wclGattClient;

	Status _status;
	std::string _deviceName;

	wclGattService _infoService, _txService, _rxService;
	wclGattCharacteristic _infoCharac, _txCharac, _rxCharac;

	sysevent_t _connectedEvent;
	syssema_t _runningCommand;

	int _vibration;

	unsigned long long _connectRetryAt;

	static const int MAX_VIBRATION_SETTING;
	static const int CONNECT_RETRY_MS;

	static const wclGattUuid GENERIC_ACCESS_SERVICE_UUID;
	static const wclGattUuid DEVICE_NAME_CHARAC_UUID;

	static const wclGattUuid INFO_SERVICE_UUID;
	static const wclGattUuid INFO_SERVICE_CHARAC_UUID;
	static const wclGattUuid TX_SERVICE_UUID, RX_SERVICE_UUID;
	static const wclGattUuid TX_CHARAC, RX_CHARAC;
};


