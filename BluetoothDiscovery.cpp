#include "BluetoothDiscovery.h"
#include "ButtplugConfig.h"
#include "Hush2Device.h"
#include "Coyote3Device.h"
#include "HismithDevice.h"

BluetoothDiscovery::BluetoothDiscovery(BtDeviceType devType, const std::string& deviceName) : _deviceType(devType), _deviceName(deviceName), _discoveredIntendedDevice(0) {
	_discoveryCompletedEvent = System::CreateEventFlag();

	__hook(&CwclBluetoothManager::OnDeviceFound, &_wclBluetoothManager, &BluetoothDiscovery::wclBluetoothManagerDeviceFound);
	__hook(&CwclBluetoothManager::OnDiscoveringCompleted, &_wclBluetoothManager, &BluetoothDiscovery::wclBluetoothManagerDiscoveringCompleted);
	__hook(&CwclBluetoothManager::OnDiscoveringStarted, &_wclBluetoothManager, &BluetoothDiscovery::wclBluetoothManagerDiscoveringStarted);

	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &BluetoothDiscovery::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &BluetoothDiscovery::wclGattClientDisconnect);

	const int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);
}

BluetoothDiscovery::~BluetoothDiscovery() {
	_wclBluetoothManager.Close();
	__unhook(&CwclBluetoothManager::OnDeviceFound, &_wclBluetoothManager, &BluetoothDiscovery::wclBluetoothManagerDeviceFound);
	__unhook(&CwclBluetoothManager::OnDiscoveringCompleted, &_wclBluetoothManager, &BluetoothDiscovery::wclBluetoothManagerDiscoveringCompleted);
	__unhook(&CwclBluetoothManager::OnDiscoveringStarted, &_wclBluetoothManager, &BluetoothDiscovery::wclBluetoothManagerDiscoveringStarted);
	__unhook(&CwclGattClient::OnConnect, &_wclGattClient, &BluetoothDiscovery::wclGattClientConnect);
	__unhook(&CwclGattClient::OnDisconnect, &_wclGattClient, &BluetoothDiscovery::wclGattClientDisconnect);
	System::DestroyEventFlag(_discoveryCompletedEvent);
}

void BluetoothDiscovery::wclBluetoothManagerDiscoveringStarted(void* Sender, CwclBluetoothRadio* const Radio) {
	_discoveredDevices.clear();
	_discoveredIntendedDevice = 0;
	log("Starting Bluetooth Discovery...\n");
}

void BluetoothDiscovery::wclBluetoothManagerDeviceFound(void* Sender, CwclBluetoothRadio* const Radio, const __int64 Address) {
	const std::string macAddressStr = MacToString(Address);
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

bool BluetoothDiscovery::runDiscovery(ButtplugConfig* config) {
	System::ResetEvent(_discoveryCompletedEvent);

	const int Res = getRadio()->Discover(10, dkBle);
	if (Res != WCL_E_SUCCESS)
		error("Error starting BT discovering: 0x%X!", Res);

	System::WaitEvent(_discoveryCompletedEvent);

	if (!_discoveredIntendedDevice)
		error("No %s devices found.\n", _deviceName.c_str());

	config->setAddress(_discoveredIntendedDevice);
	config->toFile();

	return true;
}

void BluetoothDiscovery::wclBluetoothManagerDiscoveringCompleted(void* Sender, CwclBluetoothRadio* const Radio, const int Error) {
	if (Error != WCL_E_SUCCESS)
		error("\nDiscovery completed with error 0x%X!\n", Error);
	else
		log("\nDiscovery completed.\n\n\n");

	if (_discoveredDevices.empty())
		error("No devices found!\n");

	inspectNextDevice();
}

void BluetoothDiscovery::inspectNextDevice() {
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

void BluetoothDiscovery::wclGattClientConnect(void* Sender, const int Error) {
	const BtAddress Address = ((CwclGattClient*)Sender)->Address;
	log("Examining device %s", MacToString(Address).c_str());

	std::string discoveredDeviceName = getDeviceName(Address);
	wclGattServices btServices;
	int Res = _wclGattClient.ReadServices(goNone, btServices);
	if (Res == WCL_E_SUCCESS) {
		if (probeDevice(Address, discoveredDeviceName, btServices)) {
			log(" - \x1B[%02Xmlooks like a %s\x1B[%02Xm!", GREEN, _deviceName.c_str(), WHITE);
			_discoveredIntendedDevice = Address;
		}
		log("\n");
	} else
		log(" - error 0x%X reading services.\n", Res);
	Res = _wclGattClient.Disconnect();
	if (Res != WCL_E_SUCCESS)
		log(" - error 0x%X disconnecting.\n", Res);
}

void BluetoothDiscovery::wclGattClientDisconnect(void* Sender, const int Reason) {
	if (_discoveredIntendedDevice)
		System::SetEvent(_discoveryCompletedEvent);
	else
		inspectNextDevice();
}

BtAddress BluetoothDiscovery::getDiscoveredDevice() const {
	return _discoveredIntendedDevice;
}

bool BluetoothDiscovery::probeDevice(BtAddress address, const std::string& gapName, wclGattServices& btServices) {
	switch (_deviceType) {
	case BTD_HUSH2:
		return HushDevice::detectHushType(btServices) >= 0;
	case BTD_COYOTE3: {
		if (gapName.compare(CoyoteDevice::DEVICE_NAME))
			return false;

		bool foundCoyoteService = false, foundBatteryService = false;
		for (const auto& service : btServices) {
			if (!service.Uuid.IsShortUuid)
				continue;
			if (service.Uuid.ShortUuid == CoyoteDevice::SERVICE_UUID.ShortUuid)
				foundCoyoteService = true;
			if (service.Uuid.ShortUuid == CoyoteDevice::BATTERY_SERVICE_UUID.ShortUuid)
				foundBatteryService = true;
		}
		return foundCoyoteService && foundBatteryService;
	}
	case BTD_HISMITH: {
		bool infoFound = false, txFound = false, rxFound = false;
		for (wclGattService& service : btServices) {
			if (service.Uuid.ShortUuid == HismithDevice::INFO_SERVICE_UUID.ShortUuid)
				infoFound = true;
			else if (service.Uuid.ShortUuid == HismithDevice::TX_SERVICE_UUID.ShortUuid)
				txFound = true;
			else if (service.Uuid.ShortUuid == HismithDevice::RX_SERVICE_UUID.ShortUuid)
				rxFound = true;

			if (infoFound && txFound && rxFound)
				return true;
		}
		return false;
	}
	default:
		return false;
	}
}
