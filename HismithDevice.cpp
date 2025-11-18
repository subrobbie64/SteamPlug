#include "HismithDevice.h"
#include "ButtplugConfig.h"
#include <algorithm>

HismithDevice::HismithDevice(ButtplugConfig& config) : ButtplugDevice(config), _infoService(), _infoCharac(), _txService(), _rxService(), _rxCharac(), _txCharac(), _deviceId(0) {
	_batteryLevel = 128;
}

HismithDevice::~HismithDevice() {
}

void HismithDevice::runCommand(unsigned char command, unsigned char value) {
	unsigned char commandBuffer[4];
	commandBuffer[0] = 0xAA;
	commandBuffer[1] = command;
	commandBuffer[2] = value;
	commandBuffer[3] = command + value;
	const int Res = _wclGattClient.WriteCharacteristicValue(_txCharac, commandBuffer, 4, plNone, wkWithoutResponse);
	if (Res != WCL_E_SUCCESS)
		debug("WriteCharacteristicValue failed 0x%X\n", Res);
}

void HismithDevice::setFuckMachineSpeed(int speed) {
	runCommand(0x04, (unsigned char)speed);
}

void HismithDevice::onClientCharacteristicChanged(const unsigned char* const Value, const unsigned long Length) {
}

unsigned short HismithDevice::getDeviceModelId() {
	unsigned char* deviceBuffer = NULL;
	unsigned long length = 2;
	int Res;
	unsigned short modelId = 0;
	if ((Res = _wclGattClient.ReadCharacteristicValue(_infoCharac, goNone, deviceBuffer, length)) == WCL_E_SUCCESS) {
		if (deviceBuffer) {
			if (length == 2)
				modelId = (deviceBuffer[0] << 8) | deviceBuffer[1];
			free(deviceBuffer);
		} else
			log("Reading returned no buffer\n");
	} else
		log("Error 0x%X reading model characteristic\n", Res);
	return modelId;
}

void HismithDevice::onConnectionEstablished() {
	int Res;
	if ((Res = _wclGattClient.FindService(INFO_SERVICE_UUID, _infoService)) != WCL_E_SUCCESS)
		log("FindService failed 0x%X!\n", Res);
	else if ((Res = _wclGattClient.FindService(TX_SERVICE_UUID, _txService)) != WCL_E_SUCCESS)
		log("FindService TX failed 0x%X!\n", Res);
	else if ((Res = _wclGattClient.FindService(RX_SERVICE_UUID, _rxService)) != WCL_E_SUCCESS)
		log("FindService RX failed 0x%X!\n", Res);
	else if ((Res = _wclGattClient.FindCharacteristic(_infoService, INFO_SERVICE_CHARAC_UUID, _infoCharac)) != WCL_E_SUCCESS)
		log("FindCharacteristic (INFO) Error: 0x%X", Res);
	else if ((Res = _wclGattClient.FindCharacteristic(_txService, TX_CHARAC, _txCharac)) != WCL_E_SUCCESS)
		log("FindCharacteristic (TX) Error: 0x%X", Res);
	else if ((Res = _wclGattClient.FindCharacteristic(_rxService, RX_CHARAC, _rxCharac)) != WCL_E_SUCCESS)
		log("FindCharacteristic (RX) Error: 0x%X", Res);
	else if ((Res = _wclGattClient.Subscribe(_rxCharac)) != WCL_E_SUCCESS)
		log("Subscribe failed Error 0x%X!\n", Res);
	else if ((Res = _wclGattClient.WriteClientConfiguration(_rxCharac, true, goNone)) != WCL_E_SUCCESS)
		log("WriteClientConfiguration->SubscribeForNotifications failed 0x%X\n", Res);
	else {
		log("Reading device id...\n");
		_deviceId = getDeviceModelId();
		const auto& deviceIt = TYPE_MAP.find(_deviceId);
		if (deviceIt != TYPE_MAP.end())
			log("Connected to Hismith device: %s (0x%04X)\n", deviceIt->second, _deviceId);
		else
			log("Connected to unknown Hismith device (0x%04X)\n", _deviceId);
		log("Setting speed 0\n");
		setFuckMachineSpeed(0);
		return; // Success
	}

	disconnect();
}

void HismithDevice::setVibrate(unsigned char effectiveVibrationPercent) {
	if (isConnected())
		setFuckMachineSpeed(std::clamp((int)effectiveVibrationPercent, 0, 100));
}

const std::map<unsigned short, const char*> HismithDevice::TYPE_MAP = {
	{ 0x1001, "AK Series - Full scale Hismith" },
	{ 0x1002, "Pro Traveler" },
	{ 0x1003, "Sex Droid" },
	{ 0x1004, "Hismith Mini" },
	{ 0x1005, "Hismith S1" },
	{ 0x1101, "Hismith S2" },
	{ 0x2001, "AIM Cup" },
	{ 0x2101, "Eropair Cup" },
	{ 0x2201, "Sinloli" },
	{ 0x3001, "Wildolo" },
	{ 0x3101, "Eropair V1" },
	{ 0x4001, "Auxfun Box" }
};

const wclGattUuid HismithDevice::INFO_SERVICE_UUID = { true, 0xFF90, { 0x0000ff90, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
const wclGattUuid HismithDevice::INFO_SERVICE_CHARAC_UUID = { true, 0xFF96, { 0x0000ff96, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };

const wclGattUuid HismithDevice::TX_SERVICE_UUID = { true, 0xFFE5, { 0x0000ffe5, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
const wclGattUuid HismithDevice::TX_CHARAC = { true, 0xFFE9, { 0x0000ffe9, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };;

const wclGattUuid HismithDevice::RX_SERVICE_UUID = { true, 0xFFE0, { 0x0000ffe0, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
const wclGattUuid HismithDevice::RX_CHARAC = { true, 0xFFE4, { 0x0000ffe4, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
