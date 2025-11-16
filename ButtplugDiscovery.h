#pragma once

class ButtplugConfig;

class ButtplugDiscovery {
public:
	virtual ~ButtplugDiscovery() {};
	virtual bool runDiscovery(ButtplugConfig* config) = 0;
};
