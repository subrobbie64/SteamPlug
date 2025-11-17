#pragma once
#include "ButtplugDevice.h"
#include "ButtplugDiscovery.h"

using namespace wclBluetooth;

class HushDiscovery : public ButtplugDiscovery {
public:
	HushDiscovery();
	~HushDiscovery();
protected:
	virtual bool probeDevice(const std::string& gapName, BtAddress address) override;
	virtual void onDiscoveryCompleted(ButtplugConfig* config, BtAddress foundDevice) override;
private:
	int getHushDeviceType(wclGattServices& services);
	
	int _discoveredHushDeviceType;
}; 
