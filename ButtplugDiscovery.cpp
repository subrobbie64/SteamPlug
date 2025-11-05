#include "ButtplugDiscovery.h"
#include "ButtplugConfig.h"
#pragma warning(disable: 4244)

ButtplugDiscovery::ButtplugDiscovery() : _discoveredHushDevice(0), _discoveredHushDeviceType(-1) {
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
	_discoveredHushDevice = 0;
	_discoveredHushDeviceType = -1;
	_discoveredHushName.clear();

	_discoveredDevices.clear();
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

ButtplugConfig* ButtplugDiscovery::runDiscovery() {
	System::ResetEvent(_discoveryCompletedEvent);

	const int Res = getRadio()->Discover(10, dkBle);
	if (Res != WCL_E_SUCCESS)
		error("Error starting BT discovering: 0x%X!", Res);

	System::WaitEvent(_discoveryCompletedEvent);

	if (_discoveredHushDevice == 0)
		return NULL;
	return new ButtplugConfig(_discoveredHushDevice, _discoveredHushDeviceType);
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
		_discoveredHushName = std::string(DevNameWide.begin(), DevNameWide.end());
		log(" - %s", _discoveredHushName.c_str());
		wclGattServices FServices;
		Res = _wclGattClient.ReadServices(goNone, FServices);
		if (Res == WCL_E_SUCCESS) {
			const int detectionResult = getHushDeviceType(Address, FServices);
			if (detectionResult >= 0) {
				log(" - \x1B[%02Xmlooks like a Hush 2\x1B[%02Xm (Type %d)!\n", GREEN, WHITE, detectionResult);
				_discoveredHushDeviceType = detectionResult;
				_discoveredHushDevice = Address;
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

void ButtplugDiscovery::wclGattClientDisconnect(void* Sender, const int Reason) {
	if (_discoveredHushDevice)
		System::SetEvent(_discoveryCompletedEvent);
	else
		inspectNextDevice();
}

int ButtplugDiscovery::getHushDeviceType(BtAddress address, wclGattServices& services) {
	for (wclGattService& service : services) {
		if (service.Uuid.IsShortUuid)
			continue;

		for (int j = 0; j < ButtplugDevice::NUM_HUSH_DEVICES; j++) {
			if (!memcmp(&service.Uuid.LongUuid, &ButtplugDevice::HUSH_DEVICE[j].serviceId.LongUuid, sizeof(GUID)))
				return j;
		}
	}
	return -1;
}

