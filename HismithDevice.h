#pragma once

#include "ButtplugDevice.h"
#include <map>

class HismithDevice : public ButtplugDevice {
public:
	HismithDevice(ButtplugConfig& config);
	virtual ~HismithDevice();

	virtual void setVibrate(unsigned char effectiveVibrationPercent);
protected:
	virtual void onConnectionEstablished();

private:
	void setFuckMachineSpeed(int speed);
	unsigned short getDeviceModelId();

	virtual void onClientCharacteristicChanged(const unsigned char* const Value, const unsigned long Length);
	wclGattService _infoService, _txService, _rxService;
	wclGattCharacteristic _infoCharac, _txCharac, _rxCharac;

	int _vibration;
	unsigned short _deviceId;

	static const std::map<unsigned short, const char*> TYPE_MAP;
public:
	static const wclGattUuid INFO_SERVICE_UUID;
	static const wclGattUuid INFO_SERVICE_CHARAC_UUID;
	static const wclGattUuid TX_SERVICE_UUID, RX_SERVICE_UUID;
	static const wclGattUuid TX_CHARAC, RX_CHARAC;
};

