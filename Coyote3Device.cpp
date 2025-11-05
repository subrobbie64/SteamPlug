#include "Coyote3Device.h"
#include <algorithm>
#include <assert.h>

#pragma comment (lib, "WclBluetoothFramework.lib")
#pragma comment(lib, "Ws2_32.lib")

std::string Mac2String(BtAddress Address);

CoyoteDevice::CoyoteDevice(ButtplugConfig &config)
	: AbstractButtDevice(config), _coyoteService(), _coyoteBatteryService(), _rxCharac(), _txCharac(), _batteryCharac(), _status(COYOTE_DISCONNECTED) {

	_connectRetryAt = 0;
	_connectedEvent = System::CreateEventFlag();
	_rumbleEvent = System::CreateEventFlag();
	
	_strengthSerial = 0;
	_expectedSerial = 0xFF;
	_channelStrength[0] = _channelStrength[1] = 0;
	_confirmedChannelStrength[0] = _confirmedChannelStrength[1] = 0;
	_levelA = _levelB = _currentPercentage = 0;
	_batteryLevel = 0;

	__hook(&CwclGattClient::OnConnect, &_wclGattClient, &CoyoteDevice::wclGattClientConnect);
	__hook(&CwclGattClient::OnDisconnect, &_wclGattClient, &CoyoteDevice::wclGattClientDisconnect);
	__hook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient, &CoyoteDevice::wclGattClientCharacteristicChanged);

	_wclBluetoothManager.SetMessageProcessing(mpAsync);
	int res = _wclBluetoothManager.Open();
	if (res != WCL_E_SUCCESS)
		error("Error opening Bluetooth manager: 0x%X", res);

	_wclGattClient.Address = config.getMacAddress();
	_wclGattClient.ConnectOnRead = true;
	_wclGattClient.ForceNotifications = false;

	_stopThread = false;
	_streamThread = System::CreateThread(CoyoteDevice::streamThreadFunc, this);
}

CoyoteDevice::~CoyoteDevice() {
	__unhook(&CwclGattClient::OnConnect, &_wclGattClient, &CoyoteDevice::wclGattClientConnect);
	__unhook(&CwclGattClient::OnDisconnect, &_wclGattClient, &CoyoteDevice::wclGattClientDisconnect);
	__unhook(&CwclGattClient::OnCharacteristicChanged, &_wclGattClient, &CoyoteDevice::wclGattClientCharacteristicChanged);

	_stopThread = true;
	System::WaitThread(_streamThread);

	System::DestroyEventFlag(_connectedEvent);
}

void CoyoteDevice::connect() {
	if (_connectRetryAt > System::GetMicros()) {
		debug("connect(): Waiting for retry delay\n");
		return;
	}

	_connectRetryAt = System::GetMicros() + CONNECT_RETRY_MS * 1000;
	debug("Trying Connect to Coyote device...\n");
	CwclBluetoothRadio* Radio;
	int Res = _wclBluetoothManager.GetLeRadio(Radio);
	if (Res != WCL_E_SUCCESS)
		error("Get working radio failed");

	System::ResetEvent(_connectedEvent);
	_status = COYOTE_CONNECTING;
	Res = _wclGattClient.Connect(Radio);
	if (Res != WCL_E_SUCCESS) {
		debug("GATT Connect error: %02X", Res);
		_status = COYOTE_DISCONNECTED;
		System::SetEvent(_connectedEvent);
	}
}

void CoyoteDevice::wclGattClientConnect(void* Sender, const int Error) {
	int Res;
	if (Error == WCL_E_SUCCESS) {
		if ((Res = _wclGattClient.FindService(SERVICE_UUID, _coyoteService)) != WCL_E_SUCCESS)
			error("FindService failed 0x%X!\n", Res);
		else if ((Res = _wclGattClient.FindCharacteristic(_coyoteService, TX_CHARACTERISTIC_UUID, _txCharac)) != WCL_E_SUCCESS)
			error("FindCharacteristic (TX) Error: 0x%X", Res);
		else if ((Res = _wclGattClient.FindCharacteristic(_coyoteService, RX_CHARACTERISTIC_UUID, _rxCharac)) != WCL_E_SUCCESS)
			error("FindCharacteristic (RX) Error: 0x%X", Res);
		else if ((Res = _wclGattClient.Subscribe(_rxCharac)) != WCL_E_SUCCESS)
			error("Subscribe failed Error 0x%X!\n", Res);
		else if ((Res = _wclGattClient.WriteClientConfiguration(_rxCharac, true, goNone)) != WCL_E_SUCCESS)
			error("WriteClientConfiguration->SubscribeForNotifications failed 0x%X\n", Res);
		else if ((Res = _wclGattClient.FindService(BATTERY_SERVICE_UUID, _coyoteBatteryService)) != WCL_E_SUCCESS)
			error("FindService->Battery failed 0x%X!\n", Res);
		else if ((Res = _wclGattClient.FindCharacteristic(_coyoteBatteryService, BATTERY_CHARACTERISTIC_UUID, _batteryCharac)) != WCL_E_SUCCESS)
			error("FindCharacteristic (Battery) Error: 0x%X", Res);
		else {
			_strengthSerial = 0;
			_expectedSerial = 0xFF;
			_channelStrength[0] = _channelStrength[1] = 0;
			_confirmedChannelStrength[0] = _confirmedChannelStrength[1] = 0;
			_levelA = _levelB = _currentPercentage = 0;
			
			unsigned char* batteryBuffer;
			unsigned long length;
			if (_wclGattClient.ReadCharacteristicValue(_batteryCharac, goNone, batteryBuffer, length) == WCL_E_SUCCESS) {
				if (batteryBuffer) {
					if (length == 1) {
						_batteryLevel = batteryBuffer[0];
					} else
						log("Unexpected response to read battery: %d bytes\n", length);	
					free(batteryBuffer);
				}
			}
			return; // Success
		}
	} else if (Error == WCL_E_BLUETOOTH_LE_DEVICE_NOT_FOUND)
		log("BTLE device not found.\n");
	else
		log("Connection failed with error 0x%X\n", Error);

	_wclGattClient.Disconnect();
	_status = COYOTE_DISCONNECTED;
	System::SetEvent(_connectedEvent);
}

void CoyoteDevice::wclGattClientCharacteristicChanged(void* Sender, const unsigned short Handle,
	const unsigned char* const Value, const unsigned long Length) {

	if ((Length == 6) && (Value[0] == 0x53)) { // Device ID & lighting configuration after connect, i.e. 53 00 93 3B 2D 05
		_status = COYOTE_CONNECTED;
		System::SetEvent(_connectedEvent);
	} else if ((Length == 4) && (Value[0] == 0x51)) { // Battery status, i.e. 51 00 10 64
		_batteryLevel = Value[3]; 
	} else if ((Length == 4) && (Value[0] == 0xB1)) { // Confirmation of 0xBF, i.e. B1 01 50 50
		if (_expectedSerial == Value[1]) {
			_confirmedChannelStrength[0] = Value[2];
			_confirmedChannelStrength[1] = Value[3];
		} else {
			printf("Unexpected serial: %02X, expected %02X\n", Value[1], _expectedSerial);
			_strengthSerial = 0;
		}
		_expectedSerial = 0xFF;
	} else {
		printf(" => UNKNOWN. RECV: ");
		for (unsigned long i = 0; i < Length; i++)
			printf("%02X ", Value[i]);
		printf("\n");
	}
}


enum SetChannelStrenthMethod {
	SCSM_NO_CHANGE = 0,
	SCSM_INCREASE = 1,
	SCSM_DECREASE = 2,
	SCSM_ABSOLUTE = 3,
};

unsigned char encodeFrequency(unsigned short frequency) {
	assert((frequency >= 10) && (frequency <= 1000));
	if (frequency <= 100)
		return (unsigned char)frequency;
	else if (frequency <= 600)
		return (unsigned char)(frequency - 100) / 5 + 100;
	else if (frequency <= 1000)
		return (unsigned char)(frequency - 600) / 10 + 200;
	else
		return 10;
}

struct ChannelWaveform { // 4x 25ms
	unsigned char frequency[4]; // 0 ~ 240
	unsigned char intensity[4]; // 0 ~ 100
};

void CoyoteDevice::configVibrate(unsigned char levelA, unsigned char levelB) {
	_levelA = levelA;
	_levelB = levelB;
}

void CoyoteDevice::getConfigVibrate(int* levelA, int* levelB) {
	*levelA = _levelA;
	*levelB = _levelB;
}

void CoyoteDevice::setVibrate(unsigned char effectiveVibrationPercent) {
	if (_currentPercentage || (_currentPercentage != effectiveVibrationPercent)) {
		_currentPercentage = effectiveVibrationPercent;
		System::SetEvent(_rumbleEvent);
	}
}

void CoyoteDevice::streamThread() {
	while (!_stopThread) {
		if (_status == COYOTE_CONNECTED) {
			unsigned char commandBuf[20];
			commandBuf[0] = 0xB0;
			commandBuf[1] = commandBuf[2] = commandBuf[3] = 0x00;
			if ((_channelStrength[0] != _confirmedChannelStrength[0]) || (_channelStrength[1] != _confirmedChannelStrength[1])) {
				_expectedSerial = 1 + (_strengthSerial % 0xF);
				_strengthSerial++;
				commandBuf[1] = (_expectedSerial << 4);
				if (_channelStrength[0] != _confirmedChannelStrength[0]) {
					commandBuf[1] |= SCSM_ABSOLUTE << 2;
					commandBuf[2] = _channelStrength[0];
				}
				if (_channelStrength[1] != _confirmedChannelStrength[1]) {
					commandBuf[1] |= SCSM_ABSOLUTE << 0;
					commandBuf[3] = _channelStrength[1];
				}
			}

			ChannelWaveform* aChWaveform = (ChannelWaveform*)(commandBuf + 4);
			ChannelWaveform* bChWaveform = (ChannelWaveform*)(commandBuf + 12);
			for (int i = 0; i < 4; i++) {
				aChWaveform->frequency[i] = 10; // encodeFrequency(100);
				aChWaveform->intensity[i] = (_levelA * _currentPercentage) / 100;
				bChWaveform->frequency[i] = 10;
				bChWaveform->intensity[i] = (_levelB * _currentPercentage) / 100;
			}
			_wclGattClient.WriteCharacteristicValue(_txCharac, commandBuf, 20, plNone, wkWithoutResponse);
			if (_currentPercentage == 0)
				System::WaitEvent(_rumbleEvent);
		}
		System::Sleep(100);
	}
}

threadReturn WINAPI CoyoteDevice::streamThreadFunc(void* arg) {
	((CoyoteDevice*)arg)->streamThread();
	return THREAD_RETURN;
}

const std::string& CoyoteDevice::getDeviceName() const {
	return DEVICE_NAME;
}

bool CoyoteDevice::isConnected() {
	if (_status == COYOTE_DISCONNECTED)
		connect();
	return _status == COYOTE_CONNECTED;
}

void CoyoteDevice::sendGlobalSettings(unsigned char aChLimit, unsigned char bChLimit, unsigned char aChFreqBalance, unsigned char bChFreqBalance, unsigned char aChFreqIntensity, unsigned char bChFreqIntensity) {
	unsigned char commandBuf[7];
	commandBuf[0] = 0xBF;
	// Channel limits 0~200
	commandBuf[1] = _channelStrength[0] = aChLimit;
	commandBuf[2] = _channelStrength[1] = bChLimit;
	// 0~255, the larger the value, the stronger the impact of the lower frequencies
	commandBuf[3] = aChFreqBalance;
	commandBuf[4] = bChFreqBalance;
	// 0~255, the pulse width. the larger the value, the stronger the impact of the lower frequencies
	commandBuf[5] = aChFreqIntensity;
	commandBuf[6] = bChFreqIntensity;
	/*printf("SEND: ");
	for (unsigned long i = 0; i < 7; i++)
		printf("%02X ", commandBuf[i]);
	printf("\n");*/
	_wclGattClient.WriteCharacteristicValue(_txCharac, commandBuf, 7, plNone, wkWithoutResponse);
}

void CoyoteDevice::wclGattClientDisconnect(void* Sender, const int Reason) {
	debug("Coyote disconnected, reason = 0x%X\n", Reason);
	_status = COYOTE_DISCONNECTED;
	System::SetEvent(_connectedEvent);
}


int CoyoteDevice::getBatteryLevel() const {
	return _batteryLevel;
}

const int CoyoteDevice::CONNECT_RETRY_MS = 2500;

const std::string CoyoteDevice::DEVICE_NAME = "47L121000";

const wclGattUuid CoyoteDevice::SERVICE_UUID = { true, 0x180c, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
const wclGattUuid CoyoteDevice::TX_CHARACTERISTIC_UUID = { true, 0x150a, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
const wclGattUuid CoyoteDevice::RX_CHARACTERISTIC_UUID = { true, 0x150b, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };

const wclGattUuid CoyoteDevice::BATTERY_SERVICE_UUID = { true, 0x180a, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
const wclGattUuid CoyoteDevice::BATTERY_CHARACTERISTIC_UUID = { true, 0x1500, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
