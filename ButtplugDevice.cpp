#include "ButtplugDevice.h"
#include <algorithm>

#pragma comment (lib, "WclBluetoothFramework.lib")
#pragma comment(lib, "Ws2_32.lib")

ButtplugDevice::ButtplugDevice(BtAddress address, const ButtplugDeviceDefinition* definition) : _definition(definition), _buttplugService(), _rxCharac(), _txCharac() {
	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &ButtplugDevice::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &ButtplugDevice::wclGattClientDisconnect);
	__hook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient,
		&ButtplugDevice::wclGattClientCharacteristicChanged);

	_batteryListener = NULL;

	_wclBluetoothManager.SetMessageProcessing(mpAsync);
	int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);
	
	_connectedEvent = System::CreateEventFlag();
	System::CreateSema(&_runningCommand, 1);

	_wclGattClient.Address = address;
	_wclGattClient.ConnectOnRead = true;
	_wclGattClient.ForceNotifications = false;
}

void ButtplugDevice::connect() {
	debug("Trying Connect to Buttplug device...\n");
	CwclBluetoothRadio* Radio;
	int Res = _wclBluetoothManager.GetLeRadio(Radio);
	if (Res != WCL_E_SUCCESS)
		error("Get working radio failed");

	System::ResetEvent(_connectedEvent);
	_status = BP_CONNECTING;
	Res = _wclGattClient.Connect(Radio);
	if (Res != WCL_E_SUCCESS)
		error("GATT Connect error: %02X", Res);
}

void ButtplugDevice::wclGattClientConnect(void* Sender, const int Error)
{
	long long Address = ((CwclGattClient*)Sender)->Address;
	if (Error == WCL_E_SUCCESS) {
		if (_wclGattClient.FindService(_definition->serviceId, _buttplugService) != WCL_E_SUCCESS)
			error("FindService failed!\n");

		int Res = _wclGattClient.FindCharacteristic(_buttplugService, _definition->txCharacteristicId, _txCharac);
		if (Res != WCL_E_SUCCESS)
			error("FindCharacteristic (TX) Error: 0x%X", Res);

		Res = _wclGattClient.FindCharacteristic(_buttplugService, _definition->rxCharacteristicId, _rxCharac);
		if (Res != WCL_E_SUCCESS)
			error("FindCharacteristic (RX) Error: 0x%X", Res);

		if (_wclGattClient.Subscribe(_rxCharac) != WCL_E_SUCCESS)
			error("Subscribe failed!\n");

		Res = _wclGattClient.WriteClientConfiguration(_rxCharac, true, goNone);
		if (Res != WCL_E_SUCCESS) {
			_wclGattClient.Disconnect();
			_status = BP_DISCONNECTED;
			debug("WriteClientConfiguration->SubscribeForNotifications failed 0x%X\n", Res);
			return;
		}

		issueCommand("DeviceType;");
	} else if (Error == WCL_E_BLUETOOTH_LE_DEVICE_NOT_FOUND) {
		debug("BTLE device not found.\n");
		_status = BP_DISCONNECTED;
		System::SetEvent(_connectedEvent);
	} else
		error("Connection failed with error 0x%X\n", Error);
}

void ButtplugDevice::wclGattClientDisconnect(void* Sender, const int Reason) {
	debug("Buttplug disconnected, reason = 0x%X\n", Reason);
	_status = BP_DISCONNECTED;
	System::SetEvent(_connectedEvent);
}

bool ButtplugDevice::issueCommand(const char* commandString) {
	System::WaitSema(&_runningCommand);
	int Res = _wclGattClient.WriteCharacteristicValue(_txCharac, (const unsigned char*)commandString, (unsigned int)strlen(commandString), plNone, wkWithoutResponse);
	if (Res != WCL_E_SUCCESS) {
		System::SignalSema(&_runningCommand);
		_wclGattClient.Disconnect();
		_status = BP_DISCONNECTED;
		if (Res == WCL_E_BLUETOOTH_LE_COMMUNICATION_FAILED) {
			debug("WriteCharacteristicValue error.\n");
			return false;
		} else
			error("\nWriteCharacteristicValue failed 0x%X\n", Res);
	}
	return true;
}

void ButtplugDevice::wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
	const unsigned char* const Value, const unsigned long Length) {

	if (memcmp(Value, "POWEROFF;", 9) == 0)
		return;

	System::SignalSema(&_runningCommand);

	if (memcmp(Value, "OK;", 3) == 0)
		return;
	
	if (isalpha(Value[0]) && (Value[1] == ':')) {
		if (Value[0] != 'Z')
			log("WARN: This does not look like a Hush device\n");
		_status = BP_CONNECTED;
		System::SetEvent(_connectedEvent);
	} else if (isdigit(Value[0]) && (Length <= 4)) {
		int batteryLevel = atoi((const char *)Value);
		if (_batteryListener != NULL)
			_batteryListener->onBatteryLevelReceived(batteryLevel);
	} else {
		std::string str((const char*)Value, Length);
		debug("Unknown response! (Length = %d): %s\n", Length, str.c_str());
	}
}

void ButtplugDevice::setVibrate(int percentage) {
	if (_status != BP_CONNECTED)
		return;
	static char commandBuffer[64];
	int vibrateSetting = std::clamp((percentage * MAX_VIBRATION_SETTING + 99) / 100, 0, MAX_VIBRATION_SETTING);
	sprintf(commandBuffer, "Vibrate:%d;", vibrateSetting);
	issueCommand(commandBuffer);
}

bool ButtplugDevice::readBatteryLevel() {
	if (_status != BP_CONNECTED)
		return false;
	return issueCommand("Battery;");
}

void ButtplugDevice::waitForConnection() {
	while (_status != BP_CONNECTED) {
		if (_status == BP_DISCONNECTED)
			connect();
		System::WaitEvent(_connectedEvent);
		if (_status != BP_CONNECTED)
			System::Sleep(1000);
	}
}

BPStatus ButtplugDevice::checkConnectionStatus() {
	if (_status == BP_DISCONNECTED)
		connect();
	return _status;
}

void ButtplugDevice::setBatteryListener(BatteryListener *listener) {
	_batteryListener = listener;
}

const int ButtplugDevice::MAX_VIBRATION_SETTING = 20;
