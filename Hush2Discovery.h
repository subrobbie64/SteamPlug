#pragma once
#include "BluetoothDiscovery.h"

using namespace wclBluetooth;

class HushDiscovery : public BluetoothDiscovery {
public:
	HushDiscovery();
	~HushDiscovery();
protected:
	virtual bool probeDevice(BtAddress address, const std::string& gapName, wclGattServices& btServices) override;
	virtual void storeAdditionalAttributes(ButtplugConfig* config) override;
private:
	int _discoveredHushDeviceType;
}; 
