#pragma once

#include "ButtplugDiscovery.h"

class ButtplugConfig {
public:
	ButtplugConfig(BtAddress macAddress, int type);

	BtAddress getMacAddress();
	const ButtplugDeviceDefinition* getButtplugDefinition();
	
	void getVibration(int* vibrateLeft, int* vibrateRight);
	void setVibration(int vibrateLeft, int vibrateRight);

	void toFile();
	static ButtplugConfig* fromFile();
private:
	ButtplugConfig(BtAddress macAddress, int type, int vibrateLeft, int vibrateRight);

	BtAddress _macAddress;
	int _type;

	int _vibrateLeft, _vibrateRight;

	static const char* CFG_FILENAME;
};