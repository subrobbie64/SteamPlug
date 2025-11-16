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

class ButtplugDevice {
public:
	ButtplugDevice(ButtplugConfig& config);
	virtual void connect() = 0;
	virtual bool isConnected() = 0;
	virtual const std::string& getDeviceName() const = 0;

	void adjustVibration(int bySmallRumble, int byBigRumble);
	void getVibrate(int* smallRumble, int* bigRumble) const;
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
