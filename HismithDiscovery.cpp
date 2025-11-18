#include "HismithDiscovery.h"
#include "ButtplugConfig.h"
#include "HismithDevice.h"

HismithDiscovery::HismithDiscovery() : ButtplugDiscovery("Hismith fuck machine") {
}

HismithDiscovery::~HismithDiscovery() {
}

bool HismithDiscovery::isHismithDevice(BtAddress address, wclGattServices& services) {
	bool infoFound = false, txFound = false, rxFound = false;
	for (wclGattService& service : services) {
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

bool HismithDiscovery::probeDevice(const std::string& gapName, BtAddress address) {
	wclGattServices FServices;
	int Res = _wclGattClient.ReadServices(goNone, FServices);
	if (Res == WCL_E_SUCCESS) {
		if (isHismithDevice(address, FServices))
			return true;
	} else
		log(" - error 0x%X reading services.", Res);
	return false;
}

void HismithDiscovery::storeAdditionalAttributes(ButtplugConfig* config) {
}
