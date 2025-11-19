#include <stdio.h>
#include <string.h>
#include "ButtplugConfig.h"
#include "BluetoothDevice.h"
#include "System.h"

#define BUTTPLUG_WEIGHT_LEFT (13 * 5)
#define BUTTPLUG_WEIGHT_RIGHT (7 * 5)

ButtplugConfig::ButtplugConfig() 
	: _address(0), _type(-1), _vibrateLeft(BUTTPLUG_WEIGHT_LEFT), _vibrateRight(BUTTPLUG_WEIGHT_RIGHT), _enableCoyote200(false), _channelA(10), _channelB(10) {
}

BtAddress ButtplugConfig::getAddress() const {
	return _address;
}

void ButtplugConfig::setAddress(BtAddress address) {
	_address = address;
}

int ButtplugConfig::getHushType() const {
	return _type;
}

void ButtplugConfig::setHushType(int type) {
	_type = type;
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
	char cfgFilename[512];
	getConfigFilename(cfgFilename);
	FILE* outf = fopen(cfgFilename, "w");
	if (outf) {
		unsigned char* address = (unsigned char*)&_address;
		fprintf(outf, "ADDRESS=%02X:%02X:%02X:%02X:%02X:%02X\n", address[5], address[4], address[3], address[2], address[1], address[0]);
		fprintf(outf, "TYPE=%d\n", _type);
		fprintf(outf, "L=%d\nR=%d\n", _vibrateLeft, _vibrateRight);
		fprintf(outf, "ENABLE_COYOTE_200=%d\n", _enableCoyote200 ? 1 : 0);
		fprintf(outf, "CHA=%d\nCHB=%d\n", _channelA, _channelB);
		fclose(outf);
	} else
		log("Unable to write config file \"%s\"\n", cfgFilename);
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
	ButtplugConfig* config = new ButtplugConfig();
	char cfgFilename[512];
	getConfigFilename(cfgFilename);
	FILE* file = fopen(cfgFilename, "r");
	if (file) {
		char line[256];
		while (fgets(line, sizeof(line), file)) {
			if (strncmp(line, "ADDRESS=", 8) == 0)
				config->setAddress(getBtAddressFromString(line + 8));
			else if (strncmp(line, "L=", 2) == 0)
				config->_vibrateLeft = atoi(line + 2);
			else if (strncmp(line, "R=", 2) == 0)
				config->_vibrateRight = atoi(line + 2);
			else if (strncmp(line, "CHA=", 4) == 0)
				config->_channelA = atoi(line + 4);
			else if (strncmp(line, "CHB=", 4) == 0)
				config->_channelB = atoi(line + 4);
			else if (strncmp(line, "TYPE=", 5) == 0)
				config->setHushType(atoi(line + 5));
			else if (strncmp(line, "ENABLE_COYOTE_200=", 17) == 0)
				config->_enableCoyote200 = (atoi(line + 17) != 0);
		}
		fclose(file);
	}
	return config;
}

void ButtplugConfig::getConfigFilename(char* buffer) {
	char fullPath[512];
	GetModuleFileName(NULL, fullPath, 512);
	char* sep = strrchr(fullPath, '\\');
	if (sep)
		sep++;
	else
		sep = fullPath;
		
	strcpy(buffer, sep);
	char *suffix = strrchr(buffer, '.');
	if (suffix)
		strcpy(suffix, ".cfg");
	else
		strcat(buffer, ".cfg");
}

