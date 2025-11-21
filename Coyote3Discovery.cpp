#include "Coyote3Discovery.h"
#include "Coyote3Device.h"

#pragma warning(disable: 4244)

CoyoteDiscovery::CoyoteDiscovery() : BluetoothDiscovery("DG-Labs Coyote 3.0") {
}

CoyoteDiscovery::~CoyoteDiscovery() {
}

bool CoyoteDiscovery::probeDevice(BtAddress address, const std::string& gapName, wclGattServices& btServices) {
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

void CoyoteDiscovery::storeAdditionalAttributes(ButtplugConfig* config) {
}
