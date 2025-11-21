#include "HismithDiscovery.h"
#include "ButtplugConfig.h"
#include "HismithDevice.h"

HismithDiscovery::HismithDiscovery() : BluetoothDiscovery("Hismith fuck machine") {
}

HismithDiscovery::~HismithDiscovery() {
}

bool HismithDiscovery::probeDevice(BtAddress address, const std::string& gapName, wclGattServices& btServices) {
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

void HismithDiscovery::storeAdditionalAttributes(ButtplugConfig* config) {
}
