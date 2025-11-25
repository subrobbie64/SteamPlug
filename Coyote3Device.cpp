#include "Coyote3Device.h"
#include <algorithm>
#include <assert.h>

#pragma comment (lib, "WclBluetoothFramework.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma warning(disable:6001)

CoyoteDevice::CoyoteDevice(ButtplugConfig &config) : ButtplugDevice(config), _coyoteService(), _coyoteBatteryService(), _rxCharac(), _txCharac(), _batteryCharac() {
	_rumbleEvent = System::CreateEventFlag();
	
	_strengthSerial = 0;
	_expectedSerial = 0xFF;
	_confirmedChannelStrength[0] = _confirmedChannelStrength[1] = 0;
	config.getChannels((unsigned char *)&_levelA, (unsigned char *)&_levelB);
	_readBatteryAt = 0;

	_stopThread = false;
	//_streamThread = System::CreateThread(CoyoteDevice::streamThreadFunc, this);
}

CoyoteDevice::~CoyoteDevice() {
	_stopThread = true;
	System::WaitThread(_streamThread);
}

bool CoyoteDevice::onConnectionEstablished() {
	int Res;
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
		_confirmedChannelStrength[0] = _confirmedChannelStrength[1] = 0;
		_levelA = _levelB = 0;

		const int maxLimit = _config.enableCoyote200() ? 200 : 100;
		sendGlobalSettings(maxLimit, maxLimit, 255, 255, 255, 255);
		return true;
	}
	return false;
}

void CoyoteDevice::onClientCharacteristicChanged(const unsigned char* const Value, const unsigned long Length) {
	if ((Length == 6) && (Value[0] == 0x53)) { // Device ID & lighting configuration after connect, i.e. 53 00 93 3B 2D 05
		_status = BT_CONNECTED;
	} else if ((Length == 4) && (Value[0] == 0x51)) { // Battery status, i.e. 51 00 10 64
		_batteryLevel = Value[3]; 
	} else if ((Length == 4) && (Value[0] == 0xB1)) { // Confirmation of 0xBF, i.e. B1 01 50 50
		if (_expectedSerial == Value[1]) {
			_confirmedChannelStrength[0] = Value[2];
			_confirmedChannelStrength[1] = Value[3];
			//_expectedSerial = 0xFF;
		} else
			log("Unexpected serial: %02X, expected %02X\n", Value[1], _expectedSerial);
	} else
		log(" => UNKNOWN: RECV: %s\n", hexString(Value, Length).c_str());
}

void CoyoteDevice::getChannelIntensity(int* levelA, int* levelB) const {
	*levelA = _levelA;
	*levelB = _levelB;
}

void CoyoteDevice::adjustChannelIntensity(int levelA, int levelB) {
	const int maxIntensity = _config.enableCoyote200() ? 200 : 100;
	_config.getChannels((unsigned char*)&_levelA, (unsigned char*)&_levelB);
	_levelA = (unsigned char)std::clamp(_levelA + levelA, 0, maxIntensity);
	_levelB = (unsigned char)std::clamp(_levelB + levelB, 0, maxIntensity);
	_config.setChannels(_levelA, _levelB);
	_config.toFile();
}

enum SetChannelStrenthMethod {
	SCSM_NO_CHANGE = 0,
	SCSM_INCREASE = 1,
	SCSM_DECREASE = 2,
	SCSM_ABSOLUTE = 3
};

struct ChannelWaveform { // 4x 25ms
	unsigned char frequency[4]; // 0 ~ 240
	unsigned char intensity[4]; // 0 ~ 100
};

void CoyoteDevice::setVibrate(unsigned char effectiveVibrationPercent) {
	_effectiveVibrationPercent = effectiveVibrationPercent;
	System::SetEvent(_rumbleEvent);

	if (isConnected()) {
		unsigned char commandBuf[20];
		commandBuf[0] = 0xB0;
		commandBuf[1] = commandBuf[2] = commandBuf[3] = 0x00;
		if ((_levelA != _confirmedChannelStrength[0]) || (_levelB != _confirmedChannelStrength[1])) {
			_expectedSerial = 1 + (_strengthSerial % 0xF);
			//_strengthSerial++;
			_strengthSerial = 0; // Don't need confirmation
			commandBuf[1] = (_expectedSerial << 4);
			if (_levelA != _confirmedChannelStrength[0]) {
				commandBuf[1] |= SCSM_ABSOLUTE << 2;
				commandBuf[2] = _levelA;
			}
			if (_levelB != _confirmedChannelStrength[1]) {
				commandBuf[1] |= SCSM_ABSOLUTE << 0;
				commandBuf[3] = _levelB;
			}
		}

		ChannelWaveform* aChWaveform = (ChannelWaveform*)(commandBuf + 4);
		ChannelWaveform* bChWaveform = (ChannelWaveform*)(commandBuf + 12);
		for (int i = 0; i < 4; i++) {
			aChWaveform->frequency[i] = 10; // encodeFrequency(100);
			aChWaveform->intensity[i] = (_levelA * _effectiveVibrationPercent) / 100;
			bChWaveform->frequency[i] = 10;
			bChWaveform->intensity[i] = (_levelB * _effectiveVibrationPercent) / 100;
		}
		if (_wclGattClient.WriteCharacteristicValue(_txCharac, commandBuf, 20, plNone, wkWithoutResponse) != WCL_E_SUCCESS)
			disconnect();

		if (_readBatteryAt < System::GetMicros()) {
			unsigned char* batteryBuffer;
			unsigned long length;
			if (_wclGattClient.ReadCharacteristicValue(_batteryCharac, goNone, batteryBuffer, length) == WCL_E_SUCCESS) {
				if (batteryBuffer) {
					if (length == 1)
						_batteryLevel = batteryBuffer[0];
					else
						log("Unexpected response to read battery: %s\n", hexString(batteryBuffer, length).c_str());
					free(batteryBuffer);
				}
			}
			_readBatteryAt = System::GetMicros() + CHECK_BATTERY_INTERVAL_MILLIS * 1000;
		}
	}
}

void CoyoteDevice::sendGlobalSettings(unsigned char aChLimit, unsigned char bChLimit, unsigned char aChFreqBalance, unsigned char bChFreqBalance, unsigned char aChFreqIntensity, unsigned char bChFreqIntensity) {
	unsigned char commandBuf[7];
	commandBuf[0] = 0xBF;
	// Channel limits 0~200
	commandBuf[1] = aChLimit;
	commandBuf[2] = bChLimit;
	// 0~255, the larger the value, the stronger the impact of the lower frequencies
	commandBuf[3] = aChFreqBalance;
	commandBuf[4] = bChFreqBalance;
	// 0~255, the pulse width. the larger the value, the stronger the impact of the lower frequencies
	commandBuf[5] = aChFreqIntensity;
	commandBuf[6] = bChFreqIntensity;
	_wclGattClient.WriteCharacteristicValue(_txCharac, commandBuf, 7, plNone, wkWithoutResponse);
}

unsigned char CoyoteDevice::encodeFrequency(unsigned short frequency) const {
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

void CoyoteDevice::streamThread() {
	while (!_stopThread) {
		if (isConnected()) {
			unsigned char commandBuf[20];
			commandBuf[0] = 0xB0;
			commandBuf[1] = commandBuf[2] = commandBuf[3] = 0x00;
			if ((_levelA != _confirmedChannelStrength[0]) || (_levelB != _confirmedChannelStrength[1])) {
				_expectedSerial = 1 + (_strengthSerial % 0xF);
				//_strengthSerial++;
				_strengthSerial = 0; // Don't need confirmation
				commandBuf[1] = (_expectedSerial << 4);
				if (_levelA != _confirmedChannelStrength[0]) {
					commandBuf[1] |= SCSM_ABSOLUTE << 2;
					commandBuf[2] = _levelA;
				}
				if (_levelB != _confirmedChannelStrength[1]) {
					commandBuf[1] |= SCSM_ABSOLUTE << 0;
					commandBuf[3] = _levelB;
				}
			}

			ChannelWaveform* aChWaveform = (ChannelWaveform*)(commandBuf + 4);
			ChannelWaveform* bChWaveform = (ChannelWaveform*)(commandBuf + 12);
			for (int i = 0; i < 4; i++) {
				aChWaveform->frequency[i] = 10; // encodeFrequency(100);
				aChWaveform->intensity[i] = (_levelA * _effectiveVibrationPercent) / 100;
				bChWaveform->frequency[i] = 10;
				bChWaveform->intensity[i] = (_levelB * _effectiveVibrationPercent) / 100;
			}
			if (_wclGattClient.WriteCharacteristicValue(_txCharac, commandBuf, 20, plNone, wkWithoutResponse) != WCL_E_SUCCESS)
				disconnect();

			if (_readBatteryAt < System::GetMicros()) {
				unsigned char* batteryBuffer;
				unsigned long length;
				if (_wclGattClient.ReadCharacteristicValue(_batteryCharac, goNone, batteryBuffer, length) == WCL_E_SUCCESS) {
					if (batteryBuffer) {
						if (length == 1)
							_batteryLevel = batteryBuffer[0];
						else
							log("Unexpected response to read battery: %s\n", hexString(batteryBuffer, length).c_str());
						free(batteryBuffer);
					}
				}
				_readBatteryAt = System::GetMicros() + CHECK_BATTERY_INTERVAL_MILLIS * 1000;
			}
		}
		System::Sleep(100);
		if (_effectiveVibrationPercent == 0)
			System::WaitEvent(_rumbleEvent);
	}
}

threadReturn WINAPI CoyoteDevice::streamThreadFunc(void* arg) {
	((CoyoteDevice*)arg)->streamThread();
	return THREAD_RETURN;
}

const int CoyoteDevice::CHECK_BATTERY_INTERVAL_MILLIS = 30 * 1000; // 30 seconds

const std::string CoyoteDevice::DEVICE_NAME = "47L121000";

const wclGattUuid CoyoteDevice::SERVICE_UUID = { true, 0x180c, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
const wclGattUuid CoyoteDevice::TX_CHARACTERISTIC_UUID = { true, 0x150a, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
const wclGattUuid CoyoteDevice::RX_CHARACTERISTIC_UUID = { true, 0x150b, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };

const wclGattUuid CoyoteDevice::BATTERY_SERVICE_UUID = { true, 0x180a, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
const wclGattUuid CoyoteDevice::BATTERY_CHARACTERISTIC_UUID = { true, 0x1500, { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } } };
