#include "ButtplugConfig.h"
#include <stdio.h>

#define BUTTPLUG_WEIGHT_LEFT (13 * 5)
#define BUTTPLUG_WEIGHT_RIGHT (7 * 5)

const char* ButtplugConfig::CFG_FILENAME = "SteamPlug.cfg";

ButtplugConfig::ButtplugConfig(BtAddress macAddress, int type) : _macAddress(macAddress), _type(type), _vibrateLeft(BUTTPLUG_WEIGHT_LEFT), _vibrateRight(BUTTPLUG_WEIGHT_RIGHT) {
}

ButtplugConfig::ButtplugConfig(BtAddress macAddress, int type, int vibrateLeft, int vibrateRight) : _macAddress(macAddress), _type(type), _vibrateLeft(vibrateLeft), _vibrateRight(vibrateRight) {
}

BtAddress ButtplugConfig::getMacAddress() {
	return _macAddress;
}

const ButtplugDeviceDefinition* ButtplugConfig::getButtplugDefinition() {
	if ((_type < 0) || (_type >= ButtplugDiscovery::NUM_HUSH_DEVICES))
		return NULL;
	return &ButtplugDiscovery::HUSH_DEVICE[_type];
}

void ButtplugConfig::getVibration(int* vibrateLeft, int* vibrateRight) {
	*vibrateLeft = _vibrateLeft;
	*vibrateRight = _vibrateRight;
}

void ButtplugConfig::setVibration(int vibrateLeft, int vibrateRight) {
	_vibrateLeft = vibrateLeft;
	_vibrateRight = vibrateRight;
}

void ButtplugConfig::toFile() {
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
	FILE* file = fopen(CFG_FILENAME, "r");
	char line[256];
	if (file) {
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
			fclose(file);

			if ((macAddress == 0) || (type < 0) || (type >= ButtplugDiscovery::NUM_HUSH_DEVICES)) {
				return NULL;
			}
			return new ButtplugConfig(macAddress, type, vibrateLeft, vibrateRight);
		}
		fclose(file);
	};
	return NULL;
}


