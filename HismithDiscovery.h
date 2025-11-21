#pragma once

#include "BluetoothDiscovery.h"

using namespace wclBluetooth;

class HismithDiscovery : public BluetoothDiscovery {
public:
	HismithDiscovery();
	~HismithDiscovery();

protected:
	virtual bool probeDevice(BtAddress address, const std::string& gapName, wclGattServices& btServices) override;
	virtual void storeAdditionalAttributes(ButtplugConfig* config) override;
};
