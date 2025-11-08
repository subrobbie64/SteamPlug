#include <stdio.h>
#include "ButtplugConfig.h"
#include "ButtplugDevice.h"
#include "System.h"

#define BUTTPLUG_WEIGHT_LEFT (13 * 5)
#define BUTTPLUG_WEIGHT_RIGHT (7 * 5)

const char* ButtplugConfig::CFG_FILENAME = "SteamPlug64.cfg";

ButtplugConfig::ButtplugConfig(BtAddress hushAddress, BtAddress coyoteAddress, int type) 
	: _hushAddress(hushAddress), _coyoteAddress(coyoteAddress), _type(type), _vibrateLeft(BUTTPLUG_WEIGHT_LEFT), _vibrateRight(BUTTPLUG_WEIGHT_RIGHT), _enableCoyote200(false), _channelA(0), _channelB(0) {
}

ButtplugConfig::ButtplugConfig(BtAddress hushAddress, int type, int vibrateLeft, int vibrateRight, BtAddress coyoteAddress, bool enable200, int channelA, int channelB) 
	: _hushAddress(hushAddress), _coyoteAddress(coyoteAddress), _type(type), _vibrateLeft(vibrateLeft), _vibrateRight(vibrateRight), _enableCoyote200(enable200), _channelA(channelA), _channelB(channelB) {
}

BtAddress ButtplugConfig::getHushAddress() const {
	return _hushAddress;
}

BtAddress ButtplugConfig::getCoyoteAddress() const {
	return _coyoteAddress;
}

int ButtplugConfig::getType() const {
	return _type;
}

bool ButtplugConfig::enableCoyote200() const {
	return _enableCoyote200;
}

void ButtplugConfig::getChannels(int* channelA, int* channelB) const {
	*channelA = _channelA;
	*channelB = _channelB;
}

void ButtplugConfig::setChannels(int channelA, int channelB) {
	_channelA = channelA;
	_channelB = channelB;
}

void ButtplugConfig::getVibration(int* vibrateLeft, int* vibrateRight) const {
	*vibrateLeft = _vibrateLeft;
	*vibrateRight = _vibrateRight;
}

void ButtplugConfig::setVibration(int vibrateLeft, int vibrateRight) {
	_vibrateLeft = vibrateLeft;
	_vibrateRight = vibrateRight;
}

void ButtplugConfig::toFile() const {
	FILE* outf = fopen(CFG_FILENAME, "w");
	if (outf) {
		unsigned char* address = (unsigned char*)&_hushAddress;
		fprintf(outf, "HUSH=%02X:%02X:%02X:%02X:%02X:%02X\n", address[5], address[4], address[3], address[2], address[1], address[0]);
		fprintf(outf, "TYPE=%d\n", _type);
		address = (unsigned char*)&_coyoteAddress;
		fprintf(outf, "COYOTE=%02X:%02X:%02X:%02X:%02X:%02X\n", address[5], address[4], address[3], address[2], address[1], address[0]);
		fprintf(outf, "L=%d\nR=%d\n", _vibrateLeft, _vibrateRight);
		fprintf(outf, "ENABLE_COYOTE_200=%d\n", _enableCoyote200 ? 1 : 0);
		fprintf(outf, "CHA=%d\nCHB=%d\n", _channelA, _channelB);
		fclose(outf);
	} else
		log("Unable to write config file \"%s\"\n", CFG_FILENAME);
}

BtAddress getBtAddressFromString(const char* str) {
	if ((str[2] != ':') || (str[5] != ':') || (str[8] != ':') || (str[11] != ':') || (str[14] != ':'))
		return 0;

	BtAddress address = 0;
	((unsigned char*)&address)[5] = (unsigned char)strtoul(str + 0, NULL, 16);
	((unsigned char*)&address)[4] = (unsigned char)strtoul(str + 3, NULL, 16);
	((unsigned char*)&address)[3] = (unsigned char)strtoul(str + 6, NULL, 16);
	((unsigned char*)&address)[2] = (unsigned char)strtoul(str + 9, NULL, 16);
	((unsigned char*)&address)[1] = (unsigned char)strtoul(str + 12, NULL, 16);
	((unsigned char*)&address)[0] = (unsigned char)strtoul(str + 15, NULL, 16);
	return address;
}

ButtplugConfig *ButtplugConfig::fromFile() {
	ButtplugConfig* resultConfig = NULL;

	FILE* file = fopen(CFG_FILENAME, "r");
	BtAddress macAddress = 0, coyoteAddress = 0;
	int type = 0, vibrateLeft, vibrateRight, channelA, channelB;
	bool enable200 = false;
	if (file) {
		char line  [256];
		while (fgets(line, sizeof(line), file) && (strlen(line) >= 19)) {
			if (strncmp(line, "HUSH=", 5) == 0)
				macAddress = getBtAddressFromString(line + 5);
			else if (strncmp(line, "TYPE=", 5) == 0)
				type = atoi(line + 5);
			else if (strncmp(line, "COYOTE=", 7) == 0)
				coyoteAddress = getBtAddressFromString(line + 7);
			else if (strncmp(line, "ENABLE_COYOTE_200=", 18) == 0)
				enable200 = atoi(line + 18) != 0;
			else if (strncmp(line, "L=", 2) == 0)
				vibrateLeft = atoi(line + 2);
			else if (strncmp(line, "R=", 2) == 0)
				vibrateRight = atoi(line + 2);
			else if (strncmp(line, "CHA=", 4) == 0)
				channelA = atoi(line + 4);
			else if (strncmp(line, "CHB=", 4) == 0)
				channelB = atoi(line + 4);
		}
		fclose(file);

		if ((macAddress && (type >= 0) && (type < ButtplugDevice::NUM_HUSH_DEVICES) ) || coyoteAddress)
			resultConfig = new ButtplugConfig(macAddress, type, vibrateLeft, vibrateRight, coyoteAddress, enable200, channelA, channelB);
	}
	return resultConfig;
}
