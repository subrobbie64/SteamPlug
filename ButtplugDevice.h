#pragma once
#include "BluetoothDevice.h"

#define BUTTPLUG_WIRED 128

class ButtplugDevice : public BluetoothDevice {
public:
	ButtplugDevice(ButtplugConfig& config);
	virtual ~ButtplugDevice();
	void adjustVibration(int bySmallRumble, int byBigRumble);
	void getVibrationIntensity(int* smallRumble, int* bigRumble) const;
	void setGamepadVibration(unsigned char smallRumble, unsigned char bigRumble);
	int getEffectiveVibration() const;
	int getBatteryLevel() const;

protected:
	virtual void setVibrate(unsigned char effectiveVibration) = 0;

	int _batteryLevel;
	unsigned char _effectiveVibrationPercent;

private:
	int _smallRumbleIntensity, _bigRumbleIntensity;
};
