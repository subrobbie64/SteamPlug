#pragma once

typedef unsigned long long BtAddress;

class ButtplugConfig {
public:
	BtAddress getAddress() const;
	void setAddress(BtAddress address);
	
	bool isValid() const;

	bool enableCoyote200() const;
	
	void getVibration(int* vibrateLeft, int* vibrateRight) const;
	void setVibration(int vibrateLeft, int vibrateRight);

	void getChannels(unsigned char* channelA, unsigned char* channelB) const;
	void setChannels(int channelA, int channelB);
	
	void toFile() const;
	static ButtplugConfig* fromFile();
private:
	ButtplugConfig();
	static void getConfigFilename(char* buffer);

	BtAddress _address;

	int _vibrateLeft, _vibrateRight;
	int _channelA, _channelB;
	bool _enableCoyote200;
};
