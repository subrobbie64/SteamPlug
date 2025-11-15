#include "ButtplugDevice.h"
#include <algorithm>

#pragma comment(lib, "WclBluetoothFramework.lib")
#pragma comment(lib, "Ws2_32.lib")

std::string Mac2String(BtAddress Address) {
	char macAddress[32];
	sprintf(macAddress, "%02X:%02X:%02X:%02X:%02X:%02X", ((unsigned char*)&Address)[5], ((unsigned char*)&Address)[4], ((unsigned char*)&Address)[3], ((unsigned char*)&Address)[2], ((unsigned char*)&Address)[1], ((unsigned char*)&Address)[0]);
	return std::string(macAddress);
}

GenericProperty::GenericProperty() : _value(0) {
}

GenericProperty::GenericProperty(unsigned char value) : _value(value) {
}

unsigned char GenericProperty::adjust(int by) {
	_value = std::clamp(_value + by, 0, 255);
	return _value;
}

unsigned char GenericProperty::get() const {
	return _value;
}

unsigned char GenericProperty::set(unsigned char value) {
	_value = value;
	return _value;
}

ButtplugDevice::ButtplugDevice(ButtplugConfig& config) : _config(config), _effectiveVibrationPercent(0) {
	int rumbleScaleLeft, rumbleScaleRight;
	_config.getVibration(&rumbleScaleLeft, &rumbleScaleRight);
	_smallRumbleIntensity.set(rumbleScaleLeft);
	_bigRumbleIntensity.set(rumbleScaleRight);
}

void ButtplugDevice::adjustVibration(int bySmallRumble, int byBigRumble) {
	_smallRumbleIntensity.adjust(bySmallRumble);
	_bigRumbleIntensity.adjust(byBigRumble);
	_config.setVibration(_smallRumbleIntensity.get(), _bigRumbleIntensity.get());
	_config.toFile();
}

void ButtplugDevice::setVibrate(unsigned char smallRumble, unsigned char bigRumble) {
	_effectiveVibrationPercent = std::clamp(((smallRumble * _smallRumbleIntensity.get() + bigRumble * _bigRumbleIntensity.get()) * 100) / (255 * 255), 0, 100);
	setVibrate(_effectiveVibrationPercent);
}

int ButtplugDevice::getEffectiveVibration() const {
	return _effectiveVibrationPercent;
}

