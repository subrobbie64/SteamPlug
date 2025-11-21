#pragma once
#include "BluetoothDiscovery.h"

using namespace wclBluetooth;

class CoyoteDiscovery : public BluetoothDiscovery {
public:
	CoyoteDiscovery();
	~CoyoteDiscovery();

protected:
	virtual bool probeDevice(BtAddress address, const std::string& gapName, wclGattServices& btServices) override;
	virtual void storeAdditionalAttributes(ButtplugConfig* config) override;
};

