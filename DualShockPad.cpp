#include "DualShockPad.h"
#include "System.h"
#include <algorithm>
#include <stdexcept>
#include "DualShockPad_HW.h"

#pragma comment(lib, "hidapi.lib")

#define DS_STARTUP_WAIT_RETRIES 40

extern void buildPacketCrc(unsigned char* packetBuffer);
extern void buildPacketCrcDS5(unsigned char* packetBuffer);

const std::set<unsigned short> DualPad::DUALSHOCK4_PRODUCT_IDS = { DUALSHOCK4_PRODUCT_ID_USB, DUALSHOCK4_PRODUCT_ID_USB_V2, DUALSHOCK4_PRODUCT_ID_DONGLE, DUALSHOCK4_PRODUCT_ID_BT };
const std::set<unsigned short> DualPad::DUALSENSE_PRODUCT_IDS = { DUALSENSE_PRODUCT_ID_USB, DUALSENSE_PRODUCT_ID_USB_V2 };

DualPad* DualPad::detectController() {
	DualPad* resultPad = nullptr;

	struct hid_device_info* devInfoList = hid_enumerate(VENDOR_ID_SONY, 0x0);
	try {
		for (struct hid_device_info* devInfo = devInfoList; devInfo != NULL; devInfo = devInfo->next) {
			if (DUALSENSE_PRODUCT_IDS.find(devInfo->product_id) != DUALSENSE_PRODUCT_IDS.end()) {
				resultPad = new DualSensePad(devInfo->vendor_id, devInfo->product_id, devInfo->serial_number);
				break;
			} else if (DUALSHOCK4_PRODUCT_IDS.find(devInfo->product_id) != DUALSHOCK4_PRODUCT_IDS.end()) {
				resultPad = new DualShockPad(devInfo->vendor_id, devInfo->product_id, devInfo->serial_number);
				break;
			}
		}
	} catch (const std::runtime_error& e) {
		log("Controller error: %s\n", e.what());
	}
	hid_free_enumeration(devInfoList);
	if (resultPad && resultPad->isError()) {
		delete resultPad;
		resultPad = NULL;
	}
	return resultPad;
}

DualPad::DualPad(unsigned short vendorId, unsigned short productId, const wchar_t* serial) : _inputBuffer(), _deviceError(false) {
	_isBluetooth = false;
	_batteryState = 0; 

	_hidDevice = hid_open(vendorId, productId, serial);
	if (!_hidDevice)
		throw std::runtime_error("Failed to open HID device");
	hid_set_nonblocking(_hidDevice, 1);

	System::CreateSema(&_deviceSema, 1);
}

DualPad::~DualPad() {
	System::DestroySema(&_deviceSema);
}

bool DualPad::isError() const {
	return _deviceError;
}

bool DualPad::getState(XUSB_REPORT* padReport) {
	bool result = false;
	System::WaitSema(&_deviceSema);
	int nBytes = hid_read(_hidDevice, _inputBuffer, HID_BUFFER_SIZE);
	if (nBytes < 0) {
		debug("Read from HID device error.\n");
		_deviceError = true;
	} else if (nBytes == 0) {
		debug("No response from HID\n");
	} else {
		convertPadState();
		memcpy(padReport, &_padState.Gamepad, sizeof(XUSB_REPORT));
		result = true;
	}
	System::SignalSema(&_deviceSema);
	return result;
}

unsigned char DualPad::getBatteryState() {
	return _batteryState;
}

void DualPad::updatePadRumble(UCHAR LargeMotor, UCHAR SmallMotor) {
	System::WaitSema(&_deviceSema);
	setRumbleColor(LargeMotor, SmallMotor, 0x0);
	System::SignalSema(&_deviceSema);
}

int DualPad::readHidWithRetries(unsigned char* buffer, int length) {
	for (int i = 0; i < DS_STARTUP_WAIT_RETRIES; i++) {
		int nBytes = hid_read(_hidDevice, buffer, length);
		if (nBytes < 0) {
			debug("Error reading HID\n");
			_deviceError = true;
			return nBytes;
		} else if (nBytes > 0) // got response
			return nBytes;
		else // wait and retry
			System::Sleep(100);
	}
	_deviceError = true;
	return 0;
}

DualShockPad::DualShockPad(unsigned short vendorId, unsigned short productId, const wchar_t* serial) : DualPad(vendorId, productId, serial) {
	int nBytes = readHidWithRetries(_inputBuffer, HID_BUFFER_SIZE);
	if (nBytes > 0) {
		_isBluetooth = (_inputBuffer[0] == DS4_REPORT_BLUETOOTH);

		setRumbleColor(0, 0, 0xFF0000);
		System::Sleep(800);
		setRumbleColor(0xFF, 0xFF, 0xFF0000);
		System::Sleep(200);
		setRumbleColor(0, 0, 0);
	} else
		debug("No DS4 found, no response\n");
}

void DualShockPad::setRumbleColor(unsigned char largeRumble, unsigned char smallRumble, unsigned int color) {	
	unsigned char buf[79];
	DS4OutputState* outputState;
	if (_isBluetooth) {
		memset(buf, 0, 79);
		buf[0] = 0xa2; // Output report header, needs to be included in crc32
		buf[1] = 0x11; // Output report 0x11
		buf[2] = 0xc0; // HID + CRC according to hid-sony
		buf[3] = 0x20; // ????
		buf[4] = 0x07; // Set blink + leds + motor
		outputState = (DS4OutputState*)(buf + 7);
	} else {
		memset(buf, 0, 32);
		buf[0] = 0x05; // report id
		buf[1] = 0xff;
		outputState = (DS4OutputState*)(buf + 4);
	}
	outputState->smallRumble = smallRumble;
	outputState->bigRumble = largeRumble;
	outputState->r = (unsigned char)(color >> 16);
	outputState->g = (unsigned char)(color >> 8);
	outputState->b = (unsigned char)color;

	int nbWritten;
	if (_isBluetooth) {
		buildPacketCrc(buf);
		nbWritten = hid_write(_hidDevice, buf + 1, 78);
	} else
		nbWritten = hid_write(_hidDevice, buf, 31);
		
	if (nbWritten < 0) {
		debug("Failed to write to HID device\n");
		_deviceError = true;
	}
}

#define DEADZONE_PERCENT 20 // 20% deadzone

static signed short translateAnalogAxis(unsigned char value) {
	int result = value - 128;
	int sign = (result < 0) ? -1 : 1;
	result = abs(result);
	result -= (127 * DEADZONE_PERCENT) / 100;
	if (result < 0)
		return 0;
	result = (result * 32768) / (127 - (127 * DEADZONE_PERCENT) / 100);
	result |= (result >> 8);
	result *= sign;
	result = std::clamp(result, -32768, 32767);
	return result;
}

static const unsigned short DPAD_TRANSLATION[] = {
	XINPUT_GAMEPAD_DPAD_UP,
	XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_RIGHT,
	XINPUT_GAMEPAD_DPAD_RIGHT,
	XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_RIGHT,
	XINPUT_GAMEPAD_DPAD_DOWN,
	XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT,
	XINPUT_GAMEPAD_DPAD_LEFT,
	XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_LEFT
};

static inline unsigned short map(unsigned char in, unsigned char bit, unsigned short out) {
	if (in & bit)
		return out;
	return 0;
}

static unsigned short mapButtons(unsigned char buttonDpad, unsigned char moreButtons) {
	unsigned short wButtons = 0;
	if ((buttonDpad & 0xF) < 8)
		wButtons = DPAD_TRANSLATION[buttonDpad & 0x7];

	wButtons |= map(buttonDpad, DS_TRIANGLE, XINPUT_GAMEPAD_Y);
	wButtons |= map(buttonDpad, DS_CIRCLE,   XINPUT_GAMEPAD_B);
	wButtons |= map(buttonDpad, DS_CROSS,    XINPUT_GAMEPAD_A);
	wButtons |= map(buttonDpad, DS_SQUARE,   XINPUT_GAMEPAD_X);

	wButtons |= map(moreButtons, DS_R3,      XINPUT_GAMEPAD_RIGHT_THUMB);
	wButtons |= map(moreButtons, DS_L3,      XINPUT_GAMEPAD_LEFT_THUMB);
	wButtons |= map(moreButtons, DS_OPTIONS, XINPUT_GAMEPAD_START);
	wButtons |= map(moreButtons, DS_SHARE,   XINPUT_GAMEPAD_BACK);
	wButtons |= map(moreButtons, DS_R1,      XINPUT_GAMEPAD_RIGHT_SHOULDER);
	wButtons |= map(moreButtons, DS_L1,      XINPUT_GAMEPAD_LEFT_SHOULDER);
	return wButtons;
}

void DualShockPad::convertPadState() {
	const DS4RawState* inState = (DS4RawState*)(_inputBuffer + 1);
	if (_isBluetooth)
		inState = (DS4RawState*)(_inputBuffer + 3);

	_padState.Gamepad.wButtons = mapButtons(inState->buttonDpad, inState->moreButtons);

	_padState.Gamepad.bLeftTrigger  = inState->leftTrigger;
	_padState.Gamepad.bRightTrigger = inState->rightTrigger;
	_padState.Gamepad.sThumbLX = translateAnalogAxis(inState->leftX);
	_padState.Gamepad.sThumbLY = translateAnalogAxis(~inState->leftY);
	_padState.Gamepad.sThumbRX = translateAnalogAxis(inState->rightX);
	_padState.Gamepad.sThumbRY = translateAnalogAxis(~inState->rightY);

	int batteryLevel = (inState->extensionBattery & 0xF) * BATTERY_MAX;
	if (inState->extensionBattery & 0x10) {
		_batteryState = BATTERY_CHARGING;
		_batteryState |= std::clamp(batteryLevel / DS_CHARGING_BATTERY_MAX, 0, 100);
	} else
		_batteryState = std::clamp(batteryLevel / DS_WIRELESS_BATTERY_MAX, 0, 100);
}

DualSensePad::DualSensePad(unsigned short vendorId, unsigned short productId, const wchar_t* serial) : DualPad(vendorId, productId, serial), _outputSeq(1) {
	int nBytes = readHidWithRetries(_inputBuffer, HID_BUFFER_SIZE);
	if (nBytes > 0) {
		_isBluetooth = (_inputBuffer[0] == 0x1F);
		setRumbleColor(0, 0, 0xFF0000);
		System::Sleep(800);
		setRumbleColor(0xFF, 0xFF, 0xFF0000);
		System::Sleep(200);
		setRumbleColor(0, 0, 0);
	} else
		log("No response from HID\n");
}

// Trigger effect, 0x26 = mode2 + mode4 + mode20
// Effect parameter 1: start of resistance section,
// Effect parameter 2
	// mode1: amount of force exerted
	// mode2: end of resistance section
	// mode4 + mode20: flags
	//   bit 2: do not pause effect when fully pressed
// Effect parameter 3: mode2: force exerted
// Effect parameter 4: mode4 + mode20: strength near release state
// Effect parameter 5: mode4 + mode20: strength near middle
// Effect parameter 6: mode4 + mode20: at pressed state
// Effect parameter 7: mode4 + mode20: effect actuation frequency in Hz
static const unsigned char DEFAULT_EFFECT[] = { 0x26, 0x90, 0xA0, 0xFF, 0x00, 0x00, 0x00, 0x00 };

void DualSensePad::setRumbleColor(unsigned char largeRumble, unsigned char smallRumble, unsigned int color) {
	unsigned char buf[79];
	unsigned char* commandBuf, *l2Effect, *r2Effect;
	if (_isBluetooth) {
		memset(buf, 0, 79);
		buf[0] = 0xA2; // Output report header, needs to be included in crc32
		buf[1] = 0x31; // Output report 0x31
		buf[2] = (_outputSeq++ & 0xF) << 4;
		buf[3] = 0x10; // DS_OUTPUT_TAG
		commandBuf = buf + 4;
		r2Effect = buf + 16;
		l2Effect = buf + 27;
	} else {
		memset(buf, 0, 48);
		buf[0] = 0x02; // report id
		commandBuf = buf + 1;
		r2Effect = buf + 11;
		l2Effect = buf + 22;
	}
	commandBuf[0] = 0xFF; // valid_flag0, bit 0: COMPATIBLE_VIBRATION, bit 1: HAPTICS_SELECT
	commandBuf[1] = 0xF7; // valid_flag1, bit 0: MIC_MUTE_LED_CONTROL_ENABLE, bit 1: POWER_SAVE_CONTROL_ENABLE, bit 2: LIGHTBAR_CONTROL_ENABLE, bit 3: RELEASE_LEDS, bit 4: PLAYER_INDICATOR_CONTROL_ENABLE
	// DualShock 4 compatibility mode.
	commandBuf[2] = smallRumble;
	commandBuf[3] = largeRumble;
	commandBuf[8] = DS5_MUTE_LED; // mute_button_led, 0: mute LED off, 1: mute LED on
	commandBuf[9] = DS5_MUTE_LED ? 0 : 0x10; // power_save_control, bit 4: POWER_SAVE_CONTROL_MIC_MUTE
	memcpy(r2Effect, DEFAULT_EFFECT, sizeof(DEFAULT_EFFECT));
	memcpy(l2Effect, DEFAULT_EFFECT, sizeof(DEFAULT_EFFECT));
	commandBuf[39] = 0x02; // bit 1: LIGHTBAR_SETUP_CONTROL_ENABLE
	commandBuf[41] = 0x02; // lightbar_setup, 1: Disable LEDs, 2: Enable LEDs
	commandBuf[43] = DS5_PLAYER_LEDS;
	commandBuf[44] = (unsigned char)(DS5_LIGHT_COLOR >> 16); // lightbar_red
	commandBuf[45] = (unsigned char)(DS5_LIGHT_COLOR >> 8); // lightbar_green
	commandBuf[46] = (unsigned char)(DS5_LIGHT_COLOR >> 0); // lightbar_green

	int nbWritten;
	if (_isBluetooth) {
		buildPacketCrcDS5(buf);
		nbWritten = hid_write(_hidDevice, buf + 1, 78);
	} else
		nbWritten = hid_write(_hidDevice, buf, 48);

	if (nbWritten < 0) {
		debug("Failed to write to HID device\n");
		_deviceError = true;
	}
}

void DualSensePad::convertPadState() {
	log("DualSensePad::convertPadState\n");
	if (_isBluetooth) {
		log("WARN: Bluetooth path taken!\n");
		convertPadStateBluetooth01();
	} else {
		log("USB path taken with report = %02X!\n", _inputBuffer[0]);
		convertPadStateUsb01();
	}
}

void DualSensePad::convertPadStateUsb01() {
	const DS5RawState* inState = (DS5RawState*)(_inputBuffer + 1);

	_padState.Gamepad.wButtons = mapButtons(inState->buttonDpad, inState->moreButtons);

	_padState.Gamepad.bLeftTrigger = inState->leftTrigger;
	_padState.Gamepad.bRightTrigger = inState->rightTrigger;
	_padState.Gamepad.sThumbLX = translateAnalogAxis(inState->leftX);
	_padState.Gamepad.sThumbLY = translateAnalogAxis(~inState->leftY);
	_padState.Gamepad.sThumbRX = translateAnalogAxis(inState->rightX);
	_padState.Gamepad.sThumbRY = translateAnalogAxis(~inState->rightY);

	if (inState->battery[1] & 0x08) // Charging bit
		_batteryState = 0xFF;
	else
		_batteryState = std::clamp(((inState->battery[0] & 0x0F) * 100) / 8, 0, 100);
}

void DualSensePad::convertPadStateBluetooth01() {
	const DS5RawStateBluetooth01* inState = (DS5RawStateBluetooth01*)(_inputBuffer + 3);

	_padState.Gamepad.wButtons = mapButtons(inState->buttonDpad, inState->moreButtons);

	_padState.Gamepad.bLeftTrigger = inState->leftTrigger;
	_padState.Gamepad.bRightTrigger = inState->rightTrigger;
	_padState.Gamepad.sThumbLX = translateAnalogAxis(inState->leftX);
	_padState.Gamepad.sThumbLY = translateAnalogAxis(~inState->leftY);
	_padState.Gamepad.sThumbRX = translateAnalogAxis(inState->rightX);
	_padState.Gamepad.sThumbRY = translateAnalogAxis(~inState->rightY);

	_batteryState = 0;
}
