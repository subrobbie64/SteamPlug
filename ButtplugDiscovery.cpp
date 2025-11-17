#include "ButtplugDiscovery.h"
#pragma warning(disable: 4244)

ButtplugDiscovery::ButtplugDiscovery(const std::string& deviceName) : _deviceName(deviceName), _discoveredIntendedDevice(0) {
	_discoveryCompletedEvent = System::CreateEventFlag();

	__hook(&CwclBluetoothManager::OnDeviceFound, &_wclBluetoothManager, &ButtplugDiscovery::wclBluetoothManagerDeviceFound);
	__hook(&CwclBluetoothManager::OnDiscoveringCompleted, &_wclBluetoothManager, &ButtplugDiscovery::wclBluetoothManagerDiscoveringCompleted);
	__hook(&CwclBluetoothManager::OnDiscoveringStarted, &_wclBluetoothManager, &ButtplugDiscovery::wclBluetoothManagerDiscoveringStarted);

	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &ButtplugDiscovery::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &ButtplugDiscovery::wclGattClientDisconnect);

	_wclBluetoothManager.SetMessageProcessing(mpAsync);

	const int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);
}

ButtplugDiscovery::~ButtplugDiscovery() {
	_wclBluetoothManager.Close();
	__unhook(&CwclBluetoothManager::OnDeviceFound, &_wclBluetoothManager, &ButtplugDiscovery::wclBluetoothManagerDeviceFound);
	__unhook(&CwclBluetoothManager::OnDiscoveringCompleted, &_wclBluetoothManager, &ButtplugDiscovery::wclBluetoothManagerDiscoveringCompleted);
	__unhook(&CwclBluetoothManager::OnDiscoveringStarted, &_wclBluetoothManager, &ButtplugDiscovery::wclBluetoothManagerDiscoveringStarted);
	__unhook(&CwclGattClient::OnConnect, &_wclGattClient, &ButtplugDiscovery::wclGattClientConnect);
	__unhook(&CwclGattClient::OnDisconnect, &_wclGattClient, &ButtplugDiscovery::wclGattClientDisconnect);
	System::DestroyEventFlag(_discoveryCompletedEvent);
}

CwclBluetoothRadio* ButtplugDiscovery::getRadio() {
	CwclBluetoothRadio* Radio;
	const int Res = _wclBluetoothManager.GetLeRadio(Radio);
	if (Res != WCL_E_SUCCESS)
		error("Get working radio failed: 0x%X", Res);
	return Radio;
}

void ButtplugDiscovery::wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio) {
	_discoveredDevices.clear();
	_discoveredIntendedDevice = 0;
	log("Starting Bluetooth Discovery...\n");
}

void ButtplugDiscovery::wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address) {
	const std::string macAddressStr = Mac2String(Address);
	wclBluetoothDeviceType DevType = dtMixed;
	const int Res = Radio->GetRemoteDeviceType(Address, DevType);
	if (Res != WCL_E_SUCCESS)
		log("Found device %s but cannot determine type.\n", macAddressStr.c_str());
	else if (DevType != dtBle)
		log("Found device %s but it's no BLE device, skipping.\n", macAddressStr.c_str());
	else {
		log("Found device %s\n", macAddressStr.c_str());
		_discoveredDevices.push_back(Address);
	}
}

bool ButtplugDiscovery::runDiscovery(ButtplugConfig* config) {
	System::ResetEvent(_discoveryCompletedEvent);

	const int Res = getRadio()->Discover(10, dkBle);
	if (Res != WCL_E_SUCCESS)
		error("Error starting BT discovering: 0x%X!", Res);

	System::WaitEvent(_discoveryCompletedEvent);

	if (!_discoveredIntendedDevice)
		error("No %s devices found.\n", _deviceName.c_str());

	onDiscoveryCompleted(config, _discoveredIntendedDevice);
	return true;
}

void ButtplugDiscovery::wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error) {
	if (Error != WCL_E_SUCCESS)
		error("\nDiscovery completed with error 0x%X!\n", Error);
	else
		log("\nDiscovery completed.\n\n\n");

	if (_discoveredDevices.empty())
		error("No devices found!\n");

	inspectNextDevice();
}

void ButtplugDiscovery::inspectNextDevice() {
	if (_discoveredDevices.empty()) {
		log("\nInspecting devices complete\n");
		System::SetEvent(_discoveryCompletedEvent);
		return;
	}

	_wclGattClient.ConnectOnRead = true;
	_wclGattClient.ForceNotifications = false;
	_wclGattClient.Address = _discoveredDevices.front();
	_discoveredDevices.pop_front();

	const int Res = _wclGattClient.Connect(getRadio());
	if (Res != WCL_E_SUCCESS)
		log("Connect error 0x%X\n", Res);
}

void ButtplugDiscovery::wclGattClientConnect(void* Sender, const int Error) {
	const BtAddress Address = ((CwclGattClient*)Sender)->Address;
	log("Examining device %s", Mac2String(Address).c_str());

	tstring DevNameWide;
	int Res = getRadio()->GetRemoteName(Address, DevNameWide);
	if (Res == WCL_E_SUCCESS) {
		std::string discoveredDeviceName = std::string(DevNameWide.begin(), DevNameWide.end());
		log(" - %s", discoveredDeviceName.c_str());
		if (probeDevice(discoveredDeviceName, Address)) {
			log(" - \x1B[%02Xmlooks like a %s\x1B[%02Xm!\n", GREEN, _deviceName.c_str(), WHITE);
			_discoveredIntendedDevice = Address;
			Res = _wclGattClient.Disconnect();
			if (Res != WCL_E_SUCCESS)
				log(" - error 0x%X disconnecting.", Res);
			log("\n");
			System::SetEvent(_discoveryCompletedEvent);
			return;
		}
	} else
		log(" - error 0x%X, device ignored.", Res);

	Res = _wclGattClient.Disconnect();
	if (Res != WCL_E_SUCCESS)
		log(" - error 0x%X disconnecting.", Res);
	log("\n");
}

void ButtplugDiscovery::wclGattClientDisconnect(void* Sender, const int Reason) {
	if (_discoveredIntendedDevice)
		System::SetEvent(_discoveryCompletedEvent);
	else
		inspectNextDevice();
}

BtAddress ButtplugDiscovery::getDiscoveredDevice() const {
	return _discoveredIntendedDevice;
}
