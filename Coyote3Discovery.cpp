#include "Coyote3Discovery.h"

#pragma warning(disable: 4244)

CoyoteDiscovery::CoyoteDiscovery() : _discoveredCoyoteAddress(0) {
	_discoveryCompletedEvent = System::CreateEventFlag();

	__hook(&CwclBluetoothManager::OnDeviceFound, &_wclBluetoothManager, &CoyoteDiscovery::wclBluetoothManagerDeviceFound);
	__hook(&CwclBluetoothManager::OnDiscoveringCompleted, &_wclBluetoothManager, &CoyoteDiscovery::wclBluetoothManagerDiscoveringCompleted);
	__hook(&CwclBluetoothManager::OnDiscoveringStarted, &_wclBluetoothManager, &CoyoteDiscovery::wclBluetoothManagerDiscoveringStarted);

	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &CoyoteDiscovery::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &CoyoteDiscovery::wclGattClientDisconnect);

	_wclBluetoothManager.SetMessageProcessing(mpAsync);

	int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);
}

CoyoteDiscovery::~CoyoteDiscovery() {
	_wclBluetoothManager.Close();

	__unhook(&CwclBluetoothManager::OnDeviceFound, &_wclBluetoothManager, &CoyoteDiscovery::wclBluetoothManagerDeviceFound);
	__unhook(&CwclBluetoothManager::OnDiscoveringCompleted, &_wclBluetoothManager, &CoyoteDiscovery::wclBluetoothManagerDiscoveringCompleted);
	__unhook(&CwclBluetoothManager::OnDiscoveringStarted, &_wclBluetoothManager, &CoyoteDiscovery::wclBluetoothManagerDiscoveringStarted);

	__unhook(&CwclGattClient::OnConnect, &_wclGattClient, &CoyoteDiscovery::wclGattClientConnect);
	__unhook(&CwclGattClient::OnDisconnect, &_wclGattClient, &CoyoteDiscovery::wclGattClientDisconnect);

	System::DestroyEventFlag(_discoveryCompletedEvent);
}

CwclBluetoothRadio* CoyoteDiscovery::getRadio() {
	CwclBluetoothRadio* Radio;
	int Res = _wclBluetoothManager.GetLeRadio(Radio);
	if (Res != WCL_E_SUCCESS)
		error("Get working radio failed");
	return Radio;
}

void CoyoteDiscovery::wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio) {
	_discoveredCoyoteAddress = 0;

	_discoveredDevices.clear();
	log("Starting Bluetooth Discovery...\n");
}

void CoyoteDiscovery::wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address) {
	std::string macAddressStr = Mac2String(Address);
	wclBluetoothDeviceType DevType = dtMixed;
	int Res = Radio->GetRemoteDeviceType(Address, DevType);
	if (Res != WCL_E_SUCCESS)
		log("Found device %s but cannot determine type.\n", macAddressStr.c_str());
	else if (DevType != dtBle)
		log("Found device %s but it's no BLE device, skipping.\n", macAddressStr.c_str());
	else {
		log("Found device %s\n", macAddressStr.c_str());
		_discoveredDevices.push_back(Address);
	}
}

bool CoyoteDiscovery::runDiscovery(ButtplugConfig* config) {
	System::ResetEvent(_discoveryCompletedEvent);

	int Res = getRadio()->Discover(10, dkBle);
	if (Res != WCL_E_SUCCESS)
		error("Error starting BT discovering: 0x%X!", Res);

	System::WaitEvent(_discoveryCompletedEvent);

	config->setCoyoteAddress(_discoveredCoyoteAddress);
	return _discoveredCoyoteAddress != 0;
}

void CoyoteDiscovery::wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error) {
	if (Error != WCL_E_SUCCESS)
		error("\nDiscovery completed with error 0x%X!\n", Error);
	else
		log("\nDiscovery completed.\n\n\n", Error);

	if (_discoveredDevices.empty())
		error("No devices found!\n");

	inspectNextDevice();
}

void CoyoteDiscovery::inspectNextDevice() {
	if (_discoveredDevices.empty()) {
		log("\nInspecting devices complete\n");
		System::SetEvent(_discoveryCompletedEvent);
		return;
	}

	_wclGattClient.ConnectOnRead = true;
	_wclGattClient.ForceNotifications = false;
	_wclGattClient.Address = _discoveredDevices.front();
	_discoveredDevices.pop_front();

	int Res = _wclGattClient.Connect(getRadio());
	if (Res != WCL_E_SUCCESS)
		log("Connect error 0x%X\n", Res);
}

void CoyoteDiscovery::wclGattClientConnect(void* Sender, const int Error) {
	BtAddress Address = ((CwclGattClient*)Sender)->Address;
	log("Examining device %s", Mac2String(Address).c_str());

	tstring DevNameWide;
	int Res = getRadio()->GetRemoteName(Address, DevNameWide);
	if (Res == WCL_E_SUCCESS) {
		std::string devName = std::string(DevNameWide.begin(), DevNameWide.end());
		log(" - %s", devName.c_str());
		if (!DevNameWide.compare(EXPECTED_DEVICE_NAME)) {
			log(" - \x1B[%02Xmlooks like a Coyote 3.0\x1B[%02Xm!\n", GREEN, WHITE);
			_discoveredCoyoteAddress = Address;
		}
	} else
		log(" - error 0x%X, device ignored.", Res);

	Res = _wclGattClient.Disconnect();
	if (Res != WCL_E_SUCCESS)
		log(" - error 0x%X disconnecting.", Res);
	log("\n");
}

void CoyoteDiscovery::wclGattClientDisconnect(void* Sender, const int Reason) {
	if (_discoveredCoyoteAddress)
		System::SetEvent(_discoveryCompletedEvent);
	else
		inspectNextDevice();
}

const tstring CoyoteDiscovery::EXPECTED_DEVICE_NAME = L"47L121000";
