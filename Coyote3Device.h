#pragma once

#include <wclBluetooth.h>
#include "System.h"
#include "ButtplugDevice.h"

using namespace wclBluetooth;

class CoyoteDevice : public ButtplugDevice {
public:
	CoyoteDevice(ButtplugConfig &config);
	virtual ~CoyoteDevice();
	
	void adjustChannelIntensity(int levelA, int levelB);
	void getChannelIntensity(int* levelA, int* levelB) const;
	virtual void setVibrate(unsigned char effectiveVibrationPercent);

protected:
	virtual bool onConnectionEstablished();
	virtual void onClientCharacteristicChanged(const unsigned char* const Value, const unsigned long Length);

private:
	unsigned char encodeFrequency(unsigned short frequency) const;
	void sendGlobalSettings(unsigned char aChLimit, unsigned char bChLimit, unsigned char aChFreqBalance, unsigned char bChFreqBalance, unsigned char aChFreqIntensity, unsigned char bChFreqIntensity);

	wclGattService _coyoteService, _coyoteBatteryService;
	wclGattCharacteristic _txCharac, _rxCharac, _batteryCharac;

	volatile unsigned char _levelA, _levelB;

	unsigned char _strengthSerial, _expectedSerial, _confirmedChannelStrength[2];

	unsigned long long _readBatteryAt;

	static const int CHECK_BATTERY_INTERVAL_MILLIS;
public:
	static const std::string DEVICE_NAME;

	static const wclGattUuid SERVICE_UUID, BATTERY_SERVICE_UUID;
	static const wclGattUuid TX_CHARACTERISTIC_UUID, RX_CHARACTERISTIC_UUID, BATTERY_CHARACTERISTIC_UUID;
};