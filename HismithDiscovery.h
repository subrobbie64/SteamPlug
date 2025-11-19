#pragma once

#include "BluetoothDiscovery.h"

using namespace wclBluetooth;

class HismithDiscovery : public BluetoothDiscovery {
public:
	HismithDiscovery();
	~HismithDiscovery();

protected:
	virtual bool probeDevice(const std::string& gapName, BtAddress address) override; 
	virtual void storeAdditionalAttributes(ButtplugConfig* config) override;
private:
	bool isHismithDevice(BtAddress address, wclGattServices& services);
};
