#pragma once
#include "ButtplugDevice.h"
#include "System.h"

struct ButtplugDeviceDefinition {
	wclGattUuid serviceId;
	wclGattUuid txCharacteristicId;
	wclGattUuid rxCharacteristicId;
};

class HushDiscovery;

class HushDevice: public ButtplugDevice {
public:
	HushDevice(ButtplugConfig& config);
	virtual ~HushDevice();

	virtual void setVibrate(unsigned char effectiveVibrationPercent);

	const std::string& getDeviceId() const;
	static bool isValidType(int type);
protected:
	virtual bool onConnectionEstablished();
	virtual void onClientCharacteristicChanged(const unsigned char* const Value, const unsigned long Length);

private:
	bool issueCommand(const char* commandString);
	void retryHandler();
	static threadReturn WINAPI retryHandlerFunc(void* arg);

	wclGattService _buttplugService;
	wclGattCharacteristic _txCharac, _rxCharac;
	std::string _deviceId;
	syssema_t _runningCommand;
	unsigned long long _readBatteryAt;

	static const int RETRY_DELAY_MILLIS, CHECK_BATTERY_INTERVAL_MILLIS;
	static const int MAX_VIBRATION_SETTING;

	static const ButtplugDeviceDefinition HUSH_DEVICE[];
	static const int NUM_HUSH_DEVICES;

	friend HushDiscovery;
};

