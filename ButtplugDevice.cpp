#include "ButtplugDevice.h"
#include <algorithm>

#pragma comment(lib, "WclBluetoothFramework.lib")
#pragma comment(lib, "Ws2_32.lib")

#define CONNECT_RETRY_MS 2500

std::string Mac2String(BtAddress Address) {
	char macAddress[32];
	sprintf(macAddress, "%02X:%02X:%02X:%02X:%02X:%02X", ((unsigned char*)&Address)[5], ((unsigned char*)&Address)[4], ((unsigned char*)&Address)[3], ((unsigned char*)&Address)[2], ((unsigned char*)&Address)[1], ((unsigned char*)&Address)[0]);
	return std::string(macAddress);
}

std::string UuidToString(wclGattUuid uuid) {
	char line[128];
	if (uuid.IsShortUuid)
		sprintf(line, "%04X", uuid.ShortUuid);
	else
		sprintf(line, "%08X-%04X-%04X-%04X-%02X%02X-%02X-%02X-%02X-%02X-%02X", uuid.LongUuid.Data1, uuid.LongUuid.Data2, uuid.LongUuid.Data3, uuid.LongUuid.Data4[0], uuid.LongUuid.Data4[1], uuid.LongUuid.Data4[2], uuid.LongUuid.Data4[3], uuid.LongUuid.Data4[4], uuid.LongUuid.Data4[5], uuid.LongUuid.Data4[6], uuid.LongUuid.Data4[7]);
	return line;
}

ButtplugDevice::ButtplugDevice(ButtplugConfig& config, BtAddress deviceAddress) : _config(config), _effectiveVibrationPercent(0), _connectRetryAt(0), _deviceName(), _batteryLevel(0), _currentDeviceVibration(0), _status(BT_DISCONNECTED) {
	_config.getVibration(&_smallRumbleIntensity, &_bigRumbleIntensity);

	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &ButtplugDevice::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &ButtplugDevice::wclGattClientDisconnect);
	__hook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient, &ButtplugDevice::wclGattClientCharacteristicChanged);

	_wclBluetoothManager.SetMessageProcessing(mpAsync);
	int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);

	_wclGattClient.Address = deviceAddress;
	_wclGattClient.ConnectOnRead = true;
	_wclGattClient.ForceNotifications = false;
}

ButtplugDevice::~ButtplugDevice() {
	__unhook(&CwclGattClient::OnConnect, &_wclGattClient, &ButtplugDevice::wclGattClientConnect);
	__unhook(&CwclGattClient::OnDisconnect, &_wclGattClient, &ButtplugDevice::wclGattClientDisconnect);
	__unhook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient, &ButtplugDevice::wclGattClientCharacteristicChanged);
	_wclBluetoothManager.Close();
}

void ButtplugDevice::adjustVibration(int bySmallRumble, int byBigRumble) {
	_smallRumbleIntensity = std::clamp(_smallRumbleIntensity + bySmallRumble, 0, 255);
	_bigRumbleIntensity = std::clamp(_bigRumbleIntensity + byBigRumble, 0, 255);
	_config.setVibration(_smallRumbleIntensity, _bigRumbleIntensity);
	_config.toFile();
}

void ButtplugDevice::setVibrate(unsigned char smallRumble, unsigned char bigRumble) {
	_effectiveVibrationPercent = std::clamp(((smallRumble * _smallRumbleIntensity + bigRumble * _bigRumbleIntensity) * 100) / (100 * 255), 0, 100);
	setVibrate(_effectiveVibrationPercent);
}

void ButtplugDevice::getVibrate(int* smallRumble, int* bigRumble) const {
	*smallRumble = _smallRumbleIntensity;
	*bigRumble = _bigRumbleIntensity;
}

int ButtplugDevice::getEffectiveVibration() const {
	return _effectiveVibrationPercent;
}

const std::string& ButtplugDevice::getDeviceName() const {
	return _deviceName;
}

void ButtplugDevice::connect() {
	if (_connectRetryAt > System::GetMicros()) {
		debug("connect(): Waiting for retry delay\n");
		return;
	}

	_connectRetryAt = System::GetMicros() + CONNECT_RETRY_MS * 1000;
	_currentDeviceVibration = 0;

	debug("Trying to connect to Buttplug device...\n");
	CwclBluetoothRadio* Radio;
	int Res = _wclBluetoothManager.GetLeRadio(Radio);
	if (Res != WCL_E_SUCCESS)
		error("Get working radio failed");

	_status = BT_CONNECTING;
	Res = _wclGattClient.Connect(Radio);
	if (Res != WCL_E_SUCCESS) {
		debug("GATT Connect error: %02X", Res);
		disconnect();
	}
}

void ButtplugDevice::disconnect() {
	_wclGattClient.Disconnect();
	_status = BT_DISCONNECTED;
}

bool ButtplugDevice::isConnected() {
	if (_status == BT_DISCONNECTED)
		connect();
	return _status == BT_CONNECTED;
}

int ButtplugDevice::getBatteryLevel() const {
	if (_status != BT_CONNECTED)
		return 0;
	return _batteryLevel;
}

std::string ButtplugDevice::getGapName() {
	std::string deviceName;

	wclGattService genericAccessService;
	if (_wclGattClient.FindService(GENERIC_ACCESS_SERVICE_UUID, genericAccessService) == WCL_E_SUCCESS) {
		wclGattCharacteristic deviceNameCharac;
		if (_wclGattClient.FindCharacteristic(genericAccessService, DEVICE_NAME_CHARAC_UUID, deviceNameCharac) == WCL_E_SUCCESS) {
			unsigned char* nameBuffer;
			unsigned long length;
			if (_wclGattClient.ReadCharacteristicValue(deviceNameCharac, goNone, nameBuffer, length) == WCL_E_SUCCESS) {
				if (nameBuffer) {
					deviceName = std::string((const char*)nameBuffer, length);
					free(nameBuffer);
				}
			}
		}
	}
	return deviceName;
}

void ButtplugDevice::wclGattClientConnect(void* Sender, const int Error) {
	if (Error == WCL_E_SUCCESS) {
		_deviceName = getGapName();
		onConnectionEstablished();
		_status = BT_CONNECTED;
		return;
	} else if (Error == WCL_E_BLUETOOTH_LE_DEVICE_NOT_FOUND)
		debug("BTLE device not found.\n");
	else
		debug("Connection failed with error 0x%X\n", Error);

	disconnect();
}

void ButtplugDevice::wclGattClientDisconnect(void* Sender, const int Reason) {
	debug("Device disconnected, reason = 0x%X\n", Reason);
	_status = BT_DISCONNECTED;
}

void ButtplugDevice::wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
	const unsigned char* const Value, const unsigned long Length) {
	onClientCharacteristicChanged(Value, Length);
}

const wclGattUuid ButtplugDevice::GENERIC_ACCESS_SERVICE_UUID = { true, 0x1800, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
const wclGattUuid ButtplugDevice::DEVICE_NAME_CHARAC_UUID = { true, 0x2A00, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
