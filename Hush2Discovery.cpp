#include "Hush2Discovery.h"
#include "ButtplugConfig.h"
#include "Hush2Device.h"
#pragma warning(disable: 4244)

HushDiscovery::HushDiscovery() : ButtplugDiscovery("Hush2 Buttplug"), _discoveredHushDeviceType(0) {
}

HushDiscovery::~HushDiscovery() {
}

bool HushDiscovery::probeDevice(const std::string& gapName, BtAddress address) {
	wclGattServices FServices;
	int Res = _wclGattClient.ReadServices(goNone, FServices);
	if (Res == WCL_E_SUCCESS) {
		_discoveredHushDeviceType = getHushDeviceType(FServices);
		if (_discoveredHushDeviceType >= 0)
			return true;
	}
	return false;
}

int HushDiscovery::getHushDeviceType(wclGattServices& services) {
	for (wclGattService& service : services) {
		if (service.Uuid.IsShortUuid)
			continue;

		for (int j = 0; j < HushDevice::NUM_HUSH_DEVICES; j++) {
			if (!memcmp(&service.Uuid.LongUuid, &HushDevice::HUSH_DEVICE[j].serviceId.LongUuid, sizeof(GUID)))
				return j;
		}
	}
	return -1;
}

void HushDiscovery::storeAdditionalAttributes(ButtplugConfig* config) {
	config->setHushType(_discoveredHushDeviceType);
}
