#include <stdio.h>
#include "ButtplugConfig.h"
#include "ButtplugDevice.h"
#include "System.h"

#define BUTTPLUG_WEIGHT_LEFT (13 * 5)
#define BUTTPLUG_WEIGHT_RIGHT (7 * 5)

const char* ButtplugConfig::CFG_FILENAME = "SteamPlug.cfg";

ButtplugConfig::ButtplugConfig(BtAddress macAddress, int type) : _macAddress(macAddress), _type(type), _vibrateLeft(BUTTPLUG_WEIGHT_LEFT), _vibrateRight(BUTTPLUG_WEIGHT_RIGHT) {
}

ButtplugConfig::ButtplugConfig(BtAddress macAddress, int type, int vibrateLeft, int vibrateRight) : _macAddress(macAddress), _type(type), _vibrateLeft(vibrateLeft), _vibrateRight(vibrateRight) {
}

BtAddress ButtplugConfig::getMacAddress() const {
	return _macAddress;
}

int ButtplugConfig::getType() const {
	return _type;
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
		unsigned char* address = (unsigned char*)&_macAddress;
		fprintf(outf, "%02X:%02X:%02X:%02X:%02X:%02X;%d\n", address[5], address[4], address[3], address[2], address[1], address[0], _type);
		fprintf(outf, "%d\n%d\n", _vibrateLeft, _vibrateRight);
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
	if (file) {
		char line[256];
		if (fgets(line, sizeof(line), file) && (strlen(line) >= 19)) {
			BtAddress macAddress = getBtAddressFromString(line);
			int type = (int)strtoul(line + 18, NULL, 10);

			int vibrateLeft = BUTTPLUG_WEIGHT_LEFT;
			int vibrateRight = BUTTPLUG_WEIGHT_RIGHT;
			if (fgets(line, sizeof(line), file) != NULL) {
				vibrateLeft = atoi(line);
				if (fgets(line, sizeof(line), file) != NULL) {
					vibrateRight = atoi(line);
				}
			}

			if (macAddress && (type >= 0) && (type < ButtplugDevice::NUM_HUSH_DEVICES))
				resultConfig = new ButtplugConfig(macAddress, type, vibrateLeft, vibrateRight);
		}
		fclose(file);
	}
	return resultConfig;
}
