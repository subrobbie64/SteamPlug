#pragma once

#include <wclBluetooth.h>
#include "System.h"
#include "ButtplugConfig.h"

using namespace wclBluetooth;

typedef unsigned long long BtAddress;

std::string Mac2String(BtAddress Address);
std::string UuidToString(wclGattUuid uuid);

class ButtplugDevice {
public:
	ButtplugDevice(ButtplugConfig& config, BtAddress deviceAddress);
	virtual ~ButtplugDevice();
	void connect();
	bool isConnected();
	const std::string& getDeviceName() const;

	void adjustVibration(int bySmallRumble, int byBigRumble);
	void getVibrate(int* smallRumble, int* bigRumble) const;
	void setVibrate(unsigned char smallRumble, unsigned char bigRumble);
	int getEffectiveVibration() const;

	int getBatteryLevel() const;
protected:
	enum Status {
		BT_DISCONNECTED,
		BT_CONNECTING,
		BT_CONNECTED
	};
	void disconnect();

	virtual void onConnectionEstablished() = 0;
	virtual void onClientCharacteristicChanged(const unsigned char* const Value, const unsigned long Length) = 0;
	virtual void setVibrate(unsigned char effectiveVibration) = 0;
	
	CwclBluetoothManager _wclBluetoothManager;
	CwclGattClient _wclGattClient;

	int _currentDeviceVibration;
	int _batteryLevel;

	ButtplugConfig& _config;
	std::string _deviceName;
	Status _status;
private:
	std::string getGapName();
	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);
	void wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
		const unsigned char* const Value, const unsigned long Length);

	unsigned long long _connectRetryAt;
	int _smallRumbleIntensity, _bigRumbleIntensity;
	unsigned char _effectiveVibrationPercent;

	static const wclGattUuid GENERIC_ACCESS_SERVICE_UUID, DEVICE_NAME_CHARAC_UUID;
};
