#include "Coyote3Discovery.h"
#include "Coyote3Device.h"

#pragma warning(disable: 4244)

CoyoteDiscovery::CoyoteDiscovery() : ButtplugDiscovery("DG-Labs Coyote 3.0") {
}

CoyoteDiscovery::~CoyoteDiscovery() {
}

bool CoyoteDiscovery::probeDevice(const std::string& gapName, BtAddress address) {
	if (gapName.compare(CoyoteDevice::DEVICE_NAME))
		return false;

	wclGattServices FServices;
	int Res = _wclGattClient.ReadServices(goNone, FServices);
	if (Res == WCL_E_SUCCESS) {
		bool foundCoyoteService = false, foundBatteryService = false;
		for (const auto& service : FServices) {
			if (!service.Uuid.IsShortUuid)
				continue;
			if (service.Uuid.ShortUuid == CoyoteDevice::SERVICE_UUID.ShortUuid)
				foundCoyoteService = true;
			if (service.Uuid.ShortUuid == CoyoteDevice::BATTERY_SERVICE_UUID.ShortUuid)
				foundBatteryService = true;
		}
		return foundCoyoteService && foundBatteryService;
	} else
		log(" - error 0x%X reading services.", Res);
	return false;
}
