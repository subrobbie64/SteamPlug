#include "ButtplugDevice.h"

#pragma comment (lib, "WclBluetoothFramework.lib")
#pragma comment(lib, "Ws2_32.lib")

ButtplugDevice::ButtplugDevice(BtAddress address, const ButtplugDeviceDefinition* definition) : _definition(definition), _buttplugService(), _rxCharac(), _txCharac() {
	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &ButtplugDevice::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &ButtplugDevice::wclGattClientDisconnect);
	__hook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient,
		&ButtplugDevice::wclGattClientCharacteristicChanged);

	_batteryCallback = NULL;
	_batteryCallbackArg = NULL;

	_wclBluetoothManager.SetMessageProcessing(mpAsync);
	int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);
	
	_connectedEvent = System::CreateEventFlag();
	System::CreateSema(&_runningCommand, 1);

	log("Trying to connect Buttplug %02X:%02X:%02X:%02X:%02X:%02X...", ((unsigned char*)&address)[5], ((unsigned char*)&address)[4], ((unsigned char*)&address)[3], ((unsigned char*)&address)[2], ((unsigned char*)&address)[1], ((unsigned char*)&address)[0]);
	_wclGattClient.Address = address;
	_wclGattClient.ConnectOnRead = true;
	_wclGattClient.ForceNotifications = false;

	connect();
}

void ButtplugDevice::connect() {
	debug("Trying Connect to Buttplug device...\n");
	CwclBluetoothRadio* Radio;
	int Res = _wclBluetoothManager.GetLeRadio(Radio);
	if (Res != WCL_E_SUCCESS)
		error("Get working radio failed");

	_status = BP_CONNECTING;
	Res = _wclGattClient.Connect(Radio);
	if (Res != WCL_E_SUCCESS)
		error("GATT Connect error: %02X", Res);
}

void ButtplugDevice::wclGattClientConnect(void* Sender, const int Error)
{
	long long Address = ((CwclGattClient*)Sender)->Address;
	if (Error != WCL_E_SUCCESS) 
		error("Connection failed with error 0x%X\n", Error);
	
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
}

void ButtplugDevice::wclGattClientDisconnect(void* Sender, const int Reason) {
	debug("Buttplug disconnected, reason = 0x%X\n", Reason);
	_status = BP_DISCONNECTED;
}

bool ButtplugDevice::issueCommand(const char* commandString) {
	System::WaitSema(&_runningCommand);
	int Res = _wclGattClient.WriteCharacteristicValue(_txCharac, (const unsigned char*)commandString, (unsigned int)strlen(commandString), plNone, wkWithoutResponse);
	if (Res != WCL_E_SUCCESS) {
		if (Res == WCL_E_BLUETOOTH_LE_COMMUNICATION_FAILED) {
			debug("WriteCharacteristicValue error.\n");
			System::SignalSema(&_runningCommand);
			_wclGattClient.Disconnect();
			_status = BP_DISCONNECTED;
			return false;
		} else
			error("\nWriteCharacteristicValue failed 0x%X\n", Res);
	}
	return true;
}

void ButtplugDevice::wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
	const unsigned char* const Value, const unsigned long Length) {

	System::SignalSema(&_runningCommand);

	std::string str((const char*)Value, Length);
	if (str.compare("OK;") == 0)
		return;

	if (isalpha(Value[0]) && (Value[1] == ':')) {
		if (Value[0] != 'Z')
			log("WARN: This does not look like a Hush device\n");
		_status = BP_CONNECTED;
		System::SetEvent(_connectedEvent);
	} else if (isdigit(Value[0]) && (Length <= 4)) {
		int batteryLevel = atoi(str.c_str());
		if (_batteryCallback != NULL)
			_batteryCallback(_batteryCallbackArg, batteryLevel);
	} else
		debug("Unknown response! (Length = %d): %s\n", Length, str.c_str());
}

void ButtplugDevice::setVibrate(int percentage) {
	if (_status != BP_CONNECTED)
		return;
	static char commandBuffer[64];
	sprintf(commandBuffer, "Vibrate:%d;", percentage);
	issueCommand(commandBuffer);
}

bool ButtplugDevice::readBatteryLevel() {
	if (_status != BP_CONNECTED)
		return false;
	return issueCommand("Battery;");
}

void ButtplugDevice::waitForConnection() {
	System::WaitEvent(_connectedEvent);
}

BPStatus ButtplugDevice::checkConnectionStatus() {
	if (_status == BP_DISCONNECTED)
		connect();
	else if (_status == BP_CONNECTING) {
		if (System::WaitEvent(_connectedEvent, 0))
			_status = BP_CONNECTED;
	}
	return _status;
}

void ButtplugDevice::setBatteryCallback(BatteryCallback callback, void* arg) {
	_batteryCallback = callback;
	_batteryCallbackArg = arg;
}

