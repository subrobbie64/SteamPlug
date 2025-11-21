#include "Hush2Discovery.h"
#include "ButtplugConfig.h"
#include "Hush2Device.h"
#pragma warning(disable: 4244)

HushDiscovery::HushDiscovery() : BluetoothDiscovery("Hush2 Buttplug"), _discoveredHushDeviceType(-1) {
}

HushDiscovery::~HushDiscovery() {
}

bool HushDiscovery::probeDevice(BtAddress address, const std::string& gapName, wclGattServices& btServices) {
	for (wclGattService& service : btServices) {
		if (service.Uuid.IsShortUuid)
			continue;

		for (int j = 0; j < HushDevice::NUM_HUSH_DEVICES; j++)
			if (!memcmp(&service.Uuid.LongUuid, &HushDevice::HUSH_DEVICE[j].serviceId.LongUuid, sizeof(GUID))) {
				_discoveredHushDeviceType = j;
				return true;
			}
	}
	return false;
}

void HushDiscovery::storeAdditionalAttributes(ButtplugConfig* config) {
	config->setHushType(_discoveredHushDeviceType);
}
