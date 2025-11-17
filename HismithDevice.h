#pragma once

#include "ButtplugDevice.h"

class HismithDevice : public ButtplugDevice {
public:
	HismithDevice(ButtplugConfig& config);
	virtual ~HismithDevice();

	virtual void setVibrate(unsigned char effectiveVibrationPercent);
protected:
	virtual void onConnectionEstablished();

private:
	void setFuckMachineSpeed(int speed);

	virtual void onClientCharacteristicChanged(const unsigned char* const Value, const unsigned long Length);
	wclGattService _infoService, _txService, _rxService;
	wclGattCharacteristic _infoCharac, _txCharac, _rxCharac;

	int _vibration;

public:
	static const wclGattUuid INFO_SERVICE_UUID;
	static const wclGattUuid INFO_SERVICE_CHARAC_UUID;
	static const wclGattUuid TX_SERVICE_UUID, RX_SERVICE_UUID;
	static const wclGattUuid TX_CHARAC, RX_CHARAC;
};

