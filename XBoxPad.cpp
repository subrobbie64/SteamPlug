#include "XBoxPad.h"
#include "ButtplugDevice.h"
#include "System.h"

#include <algorithm>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "XInput.lib")
#pragma comment(lib, "Xinput9_1_0.lib")

XBoxPad::XBoxPad(DWORD physicalPadId) : _physicalPadId(physicalPadId) {
    _deviceError = (_physicalPadId < 0);
}

XBoxPad::~XBoxPad() {
}

DWORD XBoxPad::getPadId() {
	return _physicalPadId;
}

bool XBoxPad::isError() const {
	return _deviceError;
}

bool XBoxPad::getState(XUSB_REPORT* padReport) {
    DWORD lastPacketId = _padState.dwPacketNumber;
    if (XInputGetState(_physicalPadId, &_padState) != ERROR_SUCCESS) {
        _deviceError = true;
        debug("Error reading from X360 pad %d!", _physicalPadId + 1);
        return false;
    }

    if (lastPacketId == _padState.dwPacketNumber)
        return false;
    
    // The XINPUT_GAMEPAD structure is identical to the XUSB_REPORT structure
    // so we can simply take it "as-is" and cast it.
	memcpy(padReport, &_padState.Gamepad, sizeof(XUSB_REPORT));
    return true;
}

unsigned char XBoxPad::getBatteryState() {
    XINPUT_BATTERY_INFORMATION batteryInfo;
	if (XInputGetBatteryInformation(_physicalPadId, BATTERY_DEVTYPE_GAMEPAD, &batteryInfo) == ERROR_SUCCESS) {
		if (batteryInfo.BatteryType == BATTERY_TYPE_WIRED)
			return BATTERY_WIRED;
		
        if (batteryInfo.BatteryLevel == BATTERY_LEVEL_LOW)
            return BATTERY_MAX / 3;
        else if (batteryInfo.BatteryLevel == BATTERY_LEVEL_MEDIUM)
            return (BATTERY_MAX * 2) / 3;
        else if (batteryInfo.BatteryLevel == BATTERY_LEVEL_FULL)
            return BATTERY_MAX;
        else // if (batteryInfo.BatteryLevel == BATTERY_LEVEL_EMPTY)
            return 0;
	}
    return 0;
}

void XBoxPad::updatePadRumble(UCHAR LargeMotor, UCHAR SmallMotor) {
    XINPUT_VIBRATION vibration;
    vibration.wLeftMotorSpeed = (static_cast<WORD>(LargeMotor) << 8) | LargeMotor;
    vibration.wRightMotorSpeed = (static_cast<WORD>(SmallMotor) << 8) | SmallMotor;
    XInputSetState(_physicalPadId, &vibration);
}

XBoxPad *XBoxPad::detectController(int virtualPadId) {
	for (int i = 0; i < XUSER_MAX_COUNT; i++) {
        if (i == virtualPadId)
            continue;
		XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS)
            return new XBoxPad(i);
	}
    return NULL;
}
