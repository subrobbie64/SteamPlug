#include "HismithDevice.h"
#include "ButtplugConfig.h"
#include <algorithm>

HismithDevice::HismithDevice(ButtplugConfig& config)
	: ButtplugDevice(config, config.getHismithAddress()), _infoService(), _infoCharac(), _txService(), _rxService(), _rxCharac(), _txCharac() {

	_vibration = 0;
	_batteryLevel = 128;
}

HismithDevice::~HismithDevice() {
}

void HismithDevice::setFuckMachineSpeed(int speed) {
	unsigned char commandBuffer[4];
	commandBuffer[0] = 0xAA;
	commandBuffer[1] = 0x4;
	commandBuffer[2] = speed;
	commandBuffer[3] = 0x4 + speed;
	const int Res = _wclGattClient.WriteCharacteristicValue(_txCharac, commandBuffer, 4, plNone, wkWithoutResponse);
	if (Res != WCL_E_SUCCESS)
		debug("WriteCharacteristicValue failed 0x%X\n", Res);
}

void HismithDevice::onClientCharacteristicChanged(const unsigned char* const Value, const unsigned long Length) {
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
		unsigned char* nameBuffer;
		unsigned long length = 2;
		if ((Res = _wclGattClient.ReadCharacteristicValue(_infoCharac, goNone, nameBuffer, length)) == WCL_E_SUCCESS) {
			if (nameBuffer) {
				if ((length == 2) && (nameBuffer[0] == 0x10) && (nameBuffer[1] == 0x05))
					log("Hismith S1 reported.\n");
				else
					error("Unexpected response to read model characteristic: %02X %02X (len = %d)\n", nameBuffer[0], nameBuffer[1], length);
				free(nameBuffer);
			} else
				log("Reading returned no buffer\n");
		} else
			log("Error 0x%X reading model characteristic\n", Res);
		log("Setting speed 0\n");
		setFuckMachineSpeed(0);
		log("Signalling connected event\n");
		return; // Success
	}

	disconnect();
}

void HismithDevice::setVibrate(unsigned char effectiveVibrationPercent) {
	if (_status != BP_CONNECTED)
		return;
	if (_vibration != effectiveVibrationPercent) {
		_vibration = effectiveVibrationPercent;
		setFuckMachineSpeed(std::clamp((int)effectiveVibrationPercent, 0, 100));
	}
}

const wclGattUuid HismithDevice::INFO_SERVICE_UUID = { true, 0xFF90, { 0x0000ff90, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
const wclGattUuid HismithDevice::INFO_SERVICE_CHARAC_UUID = { true, 0xFF96, { 0x0000ff96, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };

const wclGattUuid HismithDevice::TX_SERVICE_UUID = { true, 0xFFE5, { 0x0000ffe5, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
const wclGattUuid HismithDevice::TX_CHARAC = { true, 0xFFE9, { 0x0000ffe9, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };;

const wclGattUuid HismithDevice::RX_SERVICE_UUID = { true, 0xFFE0, { 0x0000ffe0, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
const wclGattUuid HismithDevice::RX_CHARAC = { true, 0xFFE4, { 0x0000ffe4, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
