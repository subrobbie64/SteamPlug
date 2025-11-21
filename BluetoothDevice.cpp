#include "BluetoothDevice.h"
#include "System.h"

#pragma comment(lib, "WclBluetoothFramework.lib")
#pragma comment(lib, "Ws2_32.lib")

#define CONNECT_RETRY_MS 2500

BluetoothDevice::BluetoothDevice(ButtplugConfig& config) : _config(config), _connectRetryAt(0), _deviceName(), _status(BT_DISCONNECTED) {
	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &BluetoothDevice::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &BluetoothDevice::wclGattClientDisconnect);
	__hook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient, &BluetoothDevice::wclGattClientCharacteristicChanged);

	int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);

	_wclGattClient.Address = config.getAddress();
	_wclGattClient.ConnectOnRead = true;
	_wclGattClient.ForceNotifications = false;
}

BluetoothDevice::~BluetoothDevice() {
	__unhook(&CwclGattClient::OnConnect, &_wclGattClient, &BluetoothDevice::wclGattClientConnect);
	__unhook(&CwclGattClient::OnDisconnect, &_wclGattClient, &BluetoothDevice::wclGattClientDisconnect);
	__unhook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient, &BluetoothDevice::wclGattClientCharacteristicChanged);
	_wclBluetoothManager.Close();
}

const std::string& BluetoothDevice::getDeviceName() const {
	return _deviceName;
}

void BluetoothDevice::connect() {
	if (_connectRetryAt <= System::GetMicros()) {
		_connectRetryAt = System::GetMicros() + CONNECT_RETRY_MS * 1000;
		_deviceName = BluetoothBase::getDeviceName(_wclGattClient.Address);

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
}

void BluetoothDevice::disconnect() {
	_wclGattClient.Disconnect();
	_status = BT_DISCONNECTED;
}

bool BluetoothDevice::isConnected() {
	if (_status == BT_DISCONNECTED)
		connect();
	return _status == BT_CONNECTED;
}

void BluetoothDevice::wclGattClientConnect(void* Sender, const int Error) {
	if (Error == WCL_E_SUCCESS) {;
		onConnectionEstablished();
		_status = BT_CONNECTED;
	} else {
		if (Error == WCL_E_BLUETOOTH_LE_DEVICE_NOT_FOUND)
			debug("BTLE device not found.\n");
		else
			debug("Connection failed with error 0x%X\n", Error);
		disconnect();
	}
}

void BluetoothDevice::wclGattClientDisconnect(void* Sender, const int Reason) {
	debug("Device disconnected, reason = 0x%X\n", Reason);
	_status = BT_DISCONNECTED;
}

void BluetoothDevice::wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
	const unsigned char* const Value, const unsigned long Length) {
	onClientCharacteristicChanged(Value, Length);
}
