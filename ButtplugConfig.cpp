#include <stdio.h>
#include <string.h>
#include "ButtplugConfig.h"
#include "BluetoothDevice.h"
#include "Hush2Device.h"
#include "System.h"

#define BUTTPLUG_WEIGHT_LEFT (13 * 5)
#define BUTTPLUG_WEIGHT_RIGHT (7 * 5)

ButtplugConfig::ButtplugConfig() 
	: _address(0), _vibrateLeft(BUTTPLUG_WEIGHT_LEFT), _vibrateRight(BUTTPLUG_WEIGHT_RIGHT), _enableCoyote200(false), _channelA(5), _channelB(5) {
}

BtAddress ButtplugConfig::getAddress() const {
	return _address;
}

void ButtplugConfig::setAddress(BtAddress address) {
	_address = address;
}

bool ButtplugConfig::isValid() const {
	return _address != 0;
}

bool ButtplugConfig::enableCoyote200() const {
	return _enableCoyote200;
}

void ButtplugConfig::getChannels(unsigned char* channelA, unsigned char* channelB) const {
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
		fprintf(outf, "ADDRESS=%s\n", BluetoothBase::MacToString(_address).c_str());
		fprintf(outf, "L=%d\nR=%d\n", _vibrateLeft, _vibrateRight);
		fprintf(outf, "ENABLE_COYOTE_200=%d\n", _enableCoyote200 ? 1 : 0);
		fprintf(outf, "CHA=%d\nCHB=%d\n", _channelA, _channelB);
		fclose(outf);
	} else
		log("Unable to write config file \"%s\"\n", cfgFilename);
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
				config->setAddress(BluetoothBase::MacFromString(line + 8));
			else if (strncmp(line, "L=", 2) == 0)
				config->_vibrateLeft = atoi(line + 2);
			else if (strncmp(line, "R=", 2) == 0)
				config->_vibrateRight = atoi(line + 2);
			else if (strncmp(line, "CHA=", 4) == 0)
				config->_channelA = atoi(line + 4);
			else if (strncmp(line, "CHB=", 4) == 0)
				config->_channelB = atoi(line + 4);
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

