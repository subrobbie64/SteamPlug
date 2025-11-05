#pragma once

typedef unsigned long long BtAddress;

class ButtplugConfig {
public:
	ButtplugConfig(BtAddress macAddress, int type);

	BtAddress getMacAddress() const;
	int getType() const;
	
	void getVibration(int* vibrateLeft, int* vibrateRight) const;
	void setVibration(int vibrateLeft, int vibrateRight);

	void toFile() const;
	static ButtplugConfig* fromFile();
private:
	ButtplugConfig(BtAddress macAddress, int type, int vibrateLeft, int vibrateRight);

	BtAddress _macAddress;
	int _type;

	int _vibrateLeft, _vibrateRight;

	static const char* CFG_FILENAME;
};