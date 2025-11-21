#include "ButtplugDevice.h"
#include <algorithm>

ButtplugDevice::ButtplugDevice(ButtplugConfig& config) : BluetoothDevice(config), _effectiveVibrationPercent(0), _batteryLevel(0) {
	_config.getVibration(&_smallRumbleIntensity, &_bigRumbleIntensity);
}

ButtplugDevice::~ButtplugDevice() {
}

void ButtplugDevice::adjustVibration(int bySmallRumble, int byBigRumble) {
	_smallRumbleIntensity = std::clamp(_smallRumbleIntensity + bySmallRumble, 0, 255);
	_bigRumbleIntensity = std::clamp(_bigRumbleIntensity + byBigRumble, 0, 255);
	_config.setVibration(_smallRumbleIntensity, _bigRumbleIntensity);
	_config.toFile();
}

void ButtplugDevice::setVibrate(unsigned char smallRumble, unsigned char bigRumble) {
	unsigned char newVibSetting = std::clamp(((smallRumble * _smallRumbleIntensity + bigRumble * _bigRumbleIntensity) * 100) / (100 * 255), 0, 100);
	if (_effectiveVibrationPercent != newVibSetting) {
		_effectiveVibrationPercent = newVibSetting;
		setVibrate(_effectiveVibrationPercent);
	}
}

void ButtplugDevice::getVibrate(int* smallRumble, int* bigRumble) const {
	*smallRumble = _smallRumbleIntensity;
	*bigRumble = _bigRumbleIntensity;
}

int ButtplugDevice::getEffectiveVibration() const {
	return _effectiveVibrationPercent;
}

int ButtplugDevice::getBatteryLevel() const {
	if (_status != BT_CONNECTED)
		return 0;
	return _batteryLevel;
}
