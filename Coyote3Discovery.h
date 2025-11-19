#pragma once
#include "BluetoothDiscovery.h"

using namespace wclBluetooth;

class CoyoteDiscovery : public BluetoothDiscovery {
public:
	CoyoteDiscovery();
	~CoyoteDiscovery();

protected:
	virtual bool probeDevice(const std::string& gapName, BtAddress address) override;
	virtual void storeAdditionalAttributes(ButtplugConfig* config) override;
};

