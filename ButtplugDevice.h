#pragma once

#include <wclBluetooth.h>
#include "System.h"
#include "ButtplugConfig.h"

using namespace wclBluetooth;

struct ButtplugDeviceDefinition {
	wclGattUuid serviceId;
	wclGattUuid txCharacteristicId;
	wclGattUuid rxCharacteristicId;
};

typedef unsigned long long BtAddress;

std::string Mac2String(BtAddress Address);

class GenericProperty {
public:
	GenericProperty();
	GenericProperty(unsigned char value);
	unsigned char adjust(int by);
	unsigned char get() const;
	unsigned char set(unsigned char value);
private:
	unsigned char _value;
};

class AbstractButtDevice {
public:
	AbstractButtDevice(ButtplugConfig& config);
	virtual void connect() = 0;
	virtual bool isConnected() = 0;
	virtual const std::string& getDeviceName() const = 0;

	void adjustVibration(int bySmallRumble, int byBigRumble);
	void setVibrate(unsigned char smallRumble, unsigned char bigRumble);
	int getEffectiveVibration() const;

	virtual int getBatteryLevel() const = 0;

	GenericProperty _smallRumbleIntensity, _bigRumbleIntensity;
protected:
	virtual void setVibrate(unsigned char effectiveVibration) = 0;
	ButtplugConfig& _config;
private:
	unsigned char _effectiveVibrationPercent;
};

class ButtplugDevice : public AbstractButtDevice {
public:
	ButtplugDevice(ButtplugConfig& config);
	~ButtplugDevice();

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