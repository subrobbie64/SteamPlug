#pragma once

typedef unsigned long long BtAddress;

class ButtplugConfig {
public:
	ButtplugConfig(BtAddress hushAddress, BtAddress coyoteAddress, int type);

	BtAddress getHushAddress() const;
	BtAddress getCoyoteAddress() const;
	int getType() const;
	bool enableCoyote200() const;
	
	void getVibration(int* vibrateLeft, int* vibrateRight) const;
	void setVibration(int vibrateLeft, int vibrateRight);

	void getChannels(int* channelA, int* channelB) const;
	void setChannels(int channelA, int channelB);

	void toFile() const;
	static ButtplugConfig* fromFile();
private:
	ButtplugConfig(BtAddress hushAddress, int type, int vibrateLeft, int vibrateRight, BtAddress coyoteAddress, bool enable200, int channelA, int channelB);

	BtAddress _hushAddress, _coyoteAddress;
	int _type;

	int _vibrateLeft, _vibrateRight;
	int _channelA, _channelB;
	bool _enableCoyote200;

	static const char* CFG_FILENAME;
};
