#pragma once

typedef unsigned long long BtAddress;

class ButtplugConfig {
public:
	ButtplugConfig(BtAddress hushAddress, BtAddress coyoteAddress, int type);

	BtAddress getHushAddress() const;
	void setHushAddress(BtAddress address);

	BtAddress getCoyoteAddress() const;
	void setCoyoteAddress(BtAddress address);

	BtAddress getHismithAddress() const;
	void setHismithAddress(BtAddress address);
	
	int getHushType() const;
	void setHushType(int type);

	bool enableCoyote200() const;
	void setEnableCoyote200(bool enable);
	
	void getVibration(int* vibrateLeft, int* vibrateRight) const;
	void setVibration(int vibrateLeft, int vibrateRight);
	void setVibrateLeft(int vibrateLeft);
	void setVibrateRight(int vibrateRight);

	void getChannels(int* channelA, int* channelB) const;
	void setChannels(int channelA, int channelB);
	void setChannelA(int channelA);
	void setChannelB(int channelB);

	void toFile() const;
	static ButtplugConfig* fromFile();
private:
	ButtplugConfig();
	ButtplugConfig(BtAddress hushAddress, int type, int vibrateLeft, int vibrateRight, BtAddress coyoteAddress, bool enable200, int channelA, int channelB);

	BtAddress _hushAddress, _coyoteAddress, _hismithAddress;
	int _type;

	int _vibrateLeft, _vibrateRight;
	int _channelA, _channelB;
	bool _enableCoyote200;

	static const char* CFG_FILENAME;
};
