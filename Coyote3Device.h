#pragma once

#include <wclBluetooth.h>
#include "System.h"
#include "ButtplugDevice.h"

using namespace wclBluetooth;

class CoyoteDevice : public AbstractButtDevice {
public:
	CoyoteDevice(ButtplugConfig &config);
	~CoyoteDevice();
	
	virtual void connect();
	virtual bool isConnected();

	void sendGlobalSettings(unsigned char aChLimit, unsigned char bChLimit, unsigned char aChFreqBalance, unsigned char bChFreqBalance, unsigned char aChFreqIntensity, unsigned char bChFreqIntensity);
	void configVibrate(unsigned char levelA, unsigned char levelB);
	void getConfigVibrate(int* levelA, int* levelB);
	virtual void setVibrate(unsigned char effectiveVibrationPercent);

	virtual int getBatteryLevel() const;

	virtual const std::string& getDeviceName() const;

private:
	enum Status {
		COYOTE_DISCONNECTED,
		COYOTE_CONNECTING,
		COYOTE_CONNECTED
	};

	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);

	void wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
		const unsigned char* const Value, const unsigned long Length);

	void streamThread();
	static threadReturn WINAPI streamThreadFunc(void* arg);

	CwclBluetoothManager _wclBluetoothManager;
	CwclGattClient _wclGattClient;

	wclGattService _coyoteService, _coyoteBatteryService;
	wclGattCharacteristic _txCharac, _rxCharac, _batteryCharac;

	sysevent_t _rumbleEvent;

	Status _status;

	volatile bool _stopThread;
	systhread_t _streamThread;

	volatile unsigned char _levelA, _levelB;
	volatile int _currentPercentage;

	unsigned char _strengthSerial, _expectedSerial, _channelStrength[2], _confirmedChannelStrength[2];

	unsigned char _batteryLevel;

	sysevent_t _connectedEvent;

	unsigned long long _connectRetryAt;

	static const int MAX_VIBRATION_SETTING;
	static const int CONNECT_RETRY_MS;

	static const std::string DEVICE_NAME;

	static const wclGattUuid SERVICE_UUID, BATTERY_SERVICE_UUID;
	static const wclGattUuid TX_CHARACTERISTIC_UUID, RX_CHARACTERISTIC_UUID, BATTERY_CHARACTERISTIC_UUID;
};