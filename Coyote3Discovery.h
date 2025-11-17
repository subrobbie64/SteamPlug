#pragma once
#include "ButtplugDiscovery.h"

using namespace wclBluetooth;

class CoyoteDiscovery : public ButtplugDiscovery {
public:
	CoyoteDiscovery();
	~CoyoteDiscovery();

protected:
	virtual bool probeDevice(const std::string& gapName, BtAddress address) override;
};

