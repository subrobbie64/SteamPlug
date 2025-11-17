#pragma once
#include "ButtplugDevice.h"
#include "ButtplugDiscovery.h"

using namespace wclBluetooth;

class HismithDiscovery : public ButtplugDiscovery {
public:
	HismithDiscovery();
	~HismithDiscovery();

protected:
	virtual bool probeDevice(const std::string& gapName, BtAddress address) override; 
private:
	bool isHismithDevice(BtAddress address, wclGattServices& services);
};
