#include "HismithDiscovery.h"
#include "ButtplugConfig.h"
#include "HismithDevice.h"
#pragma warning(disable: 4244)

HismithDiscovery::HismithDiscovery() : _discoveredHismithDevice(0) {
	_discoveryCompletedEvent = System::CreateEventFlag();

	__hook(&CwclBluetoothManager::OnDeviceFound, &_wclBluetoothManager, &HismithDiscovery::wclBluetoothManagerDeviceFound);
	__hook(&CwclBluetoothManager::OnDiscoveringCompleted, &_wclBluetoothManager, &HismithDiscovery::wclBluetoothManagerDiscoveringCompleted);
	__hook(&CwclBluetoothManager::OnDiscoveringStarted, &_wclBluetoothManager, &HismithDiscovery::wclBluetoothManagerDiscoveringStarted);

	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &HismithDiscovery::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &HismithDiscovery::wclGattClientDisconnect);

	_wclBluetoothManager.SetMessageProcessing(mpAsync);

	const int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);
}

HismithDiscovery::~HismithDiscovery() {
	_wclBluetoothManager.Close();

	__unhook(&CwclBluetoothManager::OnDeviceFound, &_wclBluetoothManager, &HismithDiscovery::wclBluetoothManagerDeviceFound);
	__unhook(&CwclBluetoothManager::OnDiscoveringCompleted, &_wclBluetoothManager, &HismithDiscovery::wclBluetoothManagerDiscoveringCompleted);
	__unhook(&CwclBluetoothManager::OnDiscoveringStarted, &_wclBluetoothManager, &HismithDiscovery::wclBluetoothManagerDiscoveringStarted);

	__unhook(&CwclGattClient::OnConnect, &_wclGattClient, &HismithDiscovery::wclGattClientConnect);
	__unhook(&CwclGattClient::OnDisconnect, &_wclGattClient, &HismithDiscovery::wclGattClientDisconnect);

	System::DestroyEventFlag(_discoveryCompletedEvent);
}

CwclBluetoothRadio* HismithDiscovery::getRadio() {
	CwclBluetoothRadio* Radio;
	const int Res = _wclBluetoothManager.GetLeRadio(Radio);
	if (Res != WCL_E_SUCCESS)
		error("Get working radio failed: 0x%X", Res);
	return Radio;
}

void HismithDiscovery::wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio) {
	_discoveredHismithDevice = 0;
	_discoveredDevices.clear();
	log("Starting Bluetooth Discovery...\n");
}

void HismithDiscovery::wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address) {
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

bool HismithDiscovery::runDiscovery(ButtplugConfig* config) {
	System::ResetEvent(_discoveryCompletedEvent);

	const int Res = getRadio()->Discover(10, dkBle);
	if (Res != WCL_E_SUCCESS)
		error("Error starting BT discovering: 0x%X!", Res);

	System::WaitEvent(_discoveryCompletedEvent);

	if (_discoveredHismithDevice) {
		config->setHismithAddress(_discoveredHismithDevice);
		return true;
	} else
		log("No Hismith devices discovered.\n");
	return false;
}

void HismithDiscovery::wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error) {
	if (Error != WCL_E_SUCCESS)
		error("\nDiscovery completed with error 0x%X!\n", Error);
	else
		log("\nDiscovery completed.\n\n\n");

	if (_discoveredDevices.empty())
		error("No devices found!\n");

	inspectNextDevice();
}

void HismithDiscovery::inspectNextDevice() {
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

void HismithDiscovery::wclGattClientConnect(void* Sender, const int Error) {
	const BtAddress Address = ((CwclGattClient*)Sender)->Address;
	log("Examining device %s", Mac2String(Address).c_str());

	tstring DevNameWide;
	int Res = getRadio()->GetRemoteName(Address, DevNameWide);
	if (Res == WCL_E_SUCCESS) {
		std::string discoveredDeviceName = std::string(DevNameWide.begin(), DevNameWide.end());
		log(" - %s", discoveredDeviceName.c_str());
		wclGattServices FServices;
		Res = _wclGattClient.ReadServices(goNone, FServices);
		if (Res == WCL_E_SUCCESS) {
			if (isHismithDevice(Address, FServices)) {
				log(" - \x1B[%02Xmlooks like a Himith fuck machine\x1B[%02Xm!\n", GREEN, WHITE);
				_discoveredHismithDevice = Address;
			}
		} else
			log(" - error 0x%X reading services.", Res);
	} else
		log(" - error 0x%X, device ignored.", Res);

	Res = _wclGattClient.Disconnect();
	if (Res != WCL_E_SUCCESS)
		log(" - error 0x%X disconnecting.", Res);
	log("\n");
}

void HismithDiscovery::wclGattClientDisconnect(void* Sender, const int Reason) {
	if (_discoveredHismithDevice)
		System::SetEvent(_discoveryCompletedEvent);
	else
		inspectNextDevice();
}

bool HismithDiscovery::isHismithDevice(BtAddress address, wclGattServices& services) {
	for (wclGattService& service : services) {
		if (service.Uuid.IsShortUuid)
			continue;

		bool infoFound = false, txFound = false, rxFound = false;
		if (!memcmp(&service.Uuid.LongUuid, &HismithDevice::INFO_SERVICE_UUID.serviceId.LongUuid, sizeof(GUID)))
			infoFound = true;

		if (!memcmp(&service.Uuid.LongUuid, &HismithDevice::TX_SERVICE_UUID.serviceId.LongUuid, sizeof(GUID)))
			txFound = true;
				
		if (!memcmp(&service.Uuid.LongUuid, &HismithDevice::RX_SERVICE_UUID.serviceId.LongUuid, sizeof(GUID)))
			rxFound = true;
		
		if (infoFound && txFound && rxFound)
			return true;
	}
	return false;
}

