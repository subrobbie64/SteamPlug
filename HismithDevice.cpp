#include "HismithDevice.h"
#include "ButtplugConfig.h"
#include <algorithm>

HismithDevice::HismithDevice(ButtplugConfig& config)
	: ButtplugDevice(config), _infoService(), _infoCharac(), _txService(), _rxService(), _rxCharac(), _txCharac(), _status(BP_DISCONNECTED), _deviceName() {

	_vibration = 0;

	_connectRetryAt = 0;
	_connectedEvent = System::CreateEventFlag();
	System::CreateSema(&_runningCommand, 1);

	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &HismithDevice::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &HismithDevice::wclGattClientDisconnect);
	__hook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient, &HismithDevice::wclGattClientCharacteristicChanged);

	_wclBluetoothManager.SetMessageProcessing(mpAsync);
	int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);

	_wclGattClient.Address = config.getHismithAddress();
	_wclGattClient.ConnectOnRead = true;
	_wclGattClient.ForceNotifications = false;
}

HismithDevice::~HismithDevice() {
	__unhook(&CwclGattClient::OnConnect, &_wclGattClient, &HismithDevice::wclGattClientConnect);
	__unhook(&CwclGattClient::OnDisconnect, &_wclGattClient, &HismithDevice::wclGattClientDisconnect);
	__unhook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient, &HismithDevice::wclGattClientCharacteristicChanged);

	System::DestroyEventFlag(_connectedEvent);
	System::DestroySema(&_runningCommand);
}

void HismithDevice::connect() {
	if (_connectRetryAt > System::GetMicros()) {
		debug("connect(): Waiting for retry delay\n");
		return;
	}

	_connectRetryAt = System::GetMicros() + CONNECT_RETRY_MS * 1000;
	_vibration = 0;

	debug("Trying to connect to fuck machine device...\n");
	CwclBluetoothRadio* Radio;
	int Res = _wclBluetoothManager.GetLeRadio(Radio);
	if (Res != WCL_E_SUCCESS)
		error("Get working radio failed");

	System::ResetEvent(_connectedEvent);
	_status = BP_CONNECTING;
	Res = _wclGattClient.Connect(Radio);
	if (Res != WCL_E_SUCCESS) {
		debug("GATT Connect error: %02X", Res);
		disconnect();
	}
}

void HismithDevice::disconnect() {
	_wclGattClient.Disconnect();
	_status = BP_DISCONNECTED;
	System::SetEvent(_connectedEvent);
}

std::string HismithDevice::getGapName() {
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

void HismithDevice::wclGattClientConnect(void* Sender, const int Error) {
	int Res;
	if (Error == WCL_E_SUCCESS) {
		_deviceName = getGapName();
		if ((Res = _wclGattClient.FindService(INFO_SERVICE_UUID, _infoService)) != WCL_E_SUCCESS)
			debug("FindService failed 0x%X!\n", Res);
		else if ((Res = _wclGattClient.FindService(INFO_SERVICE_CHARAC_UUID, _infoService)) != WCL_E_SUCCESS)
			debug("FindService failed 0x%X!\n", Res);
		else if ((Res = _wclGattClient.FindService(TX_SERVICE_UUID, _txService)) != WCL_E_SUCCESS)
			debug("FindService failed 0x%X!\n", Res);
		else if ((Res = _wclGattClient.FindService(RX_SERVICE_UUID, _rxService)) != WCL_E_SUCCESS)
			debug("FindService failed 0x%X!\n", Res);
		else if ((Res = _wclGattClient.FindCharacteristic(_infoService, INFO_SERVICE_CHARAC_UUID, _infoCharac)) != WCL_E_SUCCESS)
			debug("FindCharacteristic (INFO) Error: 0x%X", Res);
		else if ((Res = _wclGattClient.FindCharacteristic(_txService, TX_SERVICE_UUID, _txCharac)) != WCL_E_SUCCESS)
			debug("FindCharacteristic (TX) Error: 0x%X", Res);
		else if ((Res = _wclGattClient.FindCharacteristic(_rxService, RX_SERVICE_UUID, _rxCharac)) != WCL_E_SUCCESS)
			debug("FindCharacteristic (RX) Error: 0x%X", Res);
		else if ((Res = _wclGattClient.Subscribe(_rxCharac)) != WCL_E_SUCCESS)
			debug("Subscribe failed Error 0x%X!\n", Res);
		else if ((Res = _wclGattClient.WriteClientConfiguration(_rxCharac, true, goNone)) != WCL_E_SUCCESS)
			debug("WriteClientConfiguration->SubscribeForNotifications failed 0x%X\n", Res);
		else {
			unsigned char* nameBuffer;
			unsigned long length = 255;
			if (_wclGattClient.ReadCharacteristicValue(_infoCharac, goNone, nameBuffer, length) == WCL_E_SUCCESS) {
				if (nameBuffer) {
					fprintf(stderr, "got %d bytes via bluetooth:\n", length);
					for (int i = 0; i < length; i++)
						fprintf(stderr, "%02X ", nameBuffer[i]);
					fprintf(stderr, "\n");
					free(nameBuffer);
				}
			}

			setFuckMachineSpeed(0);
			return; // Success
		}
	} else if (Error == WCL_E_BLUETOOTH_LE_DEVICE_NOT_FOUND)
		debug("BTLE device not found.\n");
	else
		debug("Connection failed with error 0x%X\n", Error);

	disconnect();
}

void HismithDevice::wclGattClientDisconnect(void* Sender, const int Reason) {
	debug("Buttplug disconnected, reason = 0x%X\n", Reason);
	_status = BP_DISCONNECTED;
	System::SetEvent(_connectedEvent);
}

void HismithDevice::wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
	const unsigned char* const Value, const unsigned long Length) {

	System::SignalSema(&_runningCommand);

	std::string response((const char*)Value, Length);
	
}

void HismithDevice::setVibrate(unsigned char effectiveVibrationPercent) {
	if (_status != BP_CONNECTED)
		return;
	if (_vibration != effectiveVibrationPercent) {
		_vibration = effectiveVibrationPercent;
		setFuckMachineSpeed(std::clamp((int)effectiveVibrationPercent, 0, 100));
	}
}

int HismithDevice::getBatteryLevel() const {
	return 128;
}

const std::string& HismithDevice::getDeviceName() const {
	return _deviceName;
}

bool HismithDevice::isConnected() {
	if (_status == BP_DISCONNECTED)
		connect();
	return _status == BP_CONNECTED;
}

const int HismithDevice::CONNECT_RETRY_MS = 2500;

const int HismithDevice::MAX_VIBRATION_SETTING = 20;

const wclGattUuid HismithDevice::GENERIC_ACCESS_SERVICE_UUID = { true, 0x1800, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
const wclGattUuid HismithDevice::DEVICE_NAME_CHARAC_UUID = { true, 0x2A00, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };

const wclGattUuid HismithDevice::INFO_SERVICE_UUID = { false, 0, { 0x0000ff90, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
const wclGattUuid HismithDevice::INFO_SERVICE_CHARAC_UUID = { false, 0, { 0x0000ff96, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };

const wclGattUuid HismithDevice::TX_SERVICE_UUID = { false, 0, { 0x0000ffe5, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
const wclGattUuid HismithDevice::TX_CHARAC = { false, 0, { 0x0000ffe9, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };;

const wclGattUuid HismithDevice::RX_SERVICE_UUID = { false, 0, { 0x0000ffe0, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
const wclGattUuid HismithDevice::RX_CHARAC = { false, 0, { 0x0000ffe4, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }} };
