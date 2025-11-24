#pragma once

#include "BluetoothBase.h"
#include "ButtplugConfig.h"

using namespace wclBluetooth;

class BluetoothDevice : public BluetoothBase {
public:
	BluetoothDevice(ButtplugConfig& config);
	virtual ~BluetoothDevice();
	void connect();
	bool isConnected() const;
	const std::string& getDeviceName() const;

protected:
	enum Status {
		BT_DISCONNECTED,
		BT_CONNECTING,
		BT_CONNECTED
	};
	void disconnect();

	virtual bool onConnectionEstablished() = 0;
	virtual void onClientCharacteristicChanged(const unsigned char* const Value, const unsigned long Length) = 0;
	
	CwclGattClient _wclGattClient;

	ButtplugConfig& _config;
	std::string _deviceName;
	Status _status;
private:
	void wclGattClientConnect(void* Sender, const int Error);
	void wclGattClientDisconnect(void* Sender, const int Reason);
	void wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
		const unsigned char* const Value, const unsigned long Length);

	unsigned long long _connectRetryAt;	
};

