#pragma once
#include "BluetoothDiscovery.h"

using namespace wclBluetooth;

class HushDiscovery : public BluetoothDiscovery {
public:
	HushDiscovery();
	~HushDiscovery();
protected:
	virtual bool probeDevice(const std::string& gapName, BtAddress address) override;
	virtual void storeAdditionalAttributes(ButtplugConfig* config) override;
private:
	int getHushDeviceType(wclGattServices& services);
	
	int _discoveredHushDeviceType;
}; 
