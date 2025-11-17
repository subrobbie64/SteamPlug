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
		const int detectionResult = getHushDeviceType(FServices);
		if (detectionResult >= 0) {
			_discoveredHushDeviceType = detectionResult;
			return true;
		}
	}
	return false;
}

void HushDiscovery::onDiscoveryCompleted(ButtplugConfig* config, BtAddress foundDevice) {
	config->setAddress(foundDevice);
	config->setHushType(_discoveredHushDeviceType);
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

