#include "ViGem360Pad.h"
#include "ButtplugDevice.h"
#include "ButtplugConfig.h"
#include "System.h"

#include <iostream>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ViGEmClient.lib")
#pragma comment(lib, "XInput.lib")
#pragma comment(lib, "Xinput9_1_0.lib")

#define MAX_RUMBLE 200

ViGem360Pad::ViGem360Pad(ButtplugDevice& buttplugDevice, ButtplugConfig &config) 
    : _client(vigem_alloc()), _outputPad(NULL), _virtualPadPlayerIndex(-1), _physicalPadId(0xFFFF), _lastPacketId(0), _buttplugDevice(buttplugDevice), _config(config), _rumbleInstructionCount(0), _padState() {

	_config.getVibration(&_rumbleScaleLeft, &_rumbleScaleRight);
    _rumbleStatusLeft = _rumbleStatusRight = _rumbleStatusPlug = 0;

    _client = vigem_alloc();
    if (_client == nullptr)
        error("vigem_alloc failed!\n");
     
    VIGEM_ERROR retval = vigem_connect(_client);
    if (!VIGEM_SUCCESS(retval))
		error("ViGEm Bus connection failed with error code: 0x%X\n", retval);

	_padIdAssigmentEvent = System::CreateEventFlag();
    
    debug("Setting up virtual x360 controller...");
    //
    // Allocate handle to identify new pad
    //
    _outputPad = vigem_target_x360_alloc();
	if (!_outputPad)
		error("vigem_target_x360_alloc failed!\n");

    retval = vigem_target_add(_client, _outputPad);
	if (!VIGEM_SUCCESS(retval))
		error("Virtual controller plug-in failed: 0x%X", retval);
    debug("okay!\n");

    retval = vigem_target_x360_register_notification(_client, _outputPad, &rumbleCallbackFn, this);
    if (!VIGEM_SUCCESS(retval))
        error("Registering for notification failed with error code: 0x%X", retval);

    waitForIdAssignment();
}

ViGem360Pad::~ViGem360Pad() {
	vigem_target_remove(_client, _outputPad);
	vigem_target_free(_outputPad);

    vigem_disconnect(_client);
    vigem_free(_client);
}

void ViGem360Pad::updateState() {
    // Grab the input from a physical X360 pad in this example
    if (XInputGetState(_physicalPadId, &_padState) != ERROR_SUCCESS)
        error("Error reading from X360 pad %d!", _physicalPadId + 1);

    if (_lastPacketId != _padState.dwPacketNumber) {
        _lastPacketId = _padState.dwPacketNumber;
        // The XINPUT_GAMEPAD structure is identical to the XUSB_REPORT structure
        // so we can simply take it "as-is" and cast it.
        //
        // Call this function on every input state change e.g. in a loop polling
        // another joystick or network device or thermometer or... you get the idea.
        vigem_target_x360_update(_client, _outputPad, *reinterpret_cast<const XUSB_REPORT*>(&_padState.Gamepad));
    }
}

void ViGem360Pad::setRumble(UCHAR LargeMotor, UCHAR SmallMotor) {
    int newLeft = (_rumbleScaleLeft * LargeMotor) / 255;
    int newRight = (_rumbleScaleRight * SmallMotor) / 255;
    if ((newLeft != _rumbleStatusLeft) || (newRight != _rumbleStatusRight)) {
        _rumbleStatusLeft = newLeft;
        _rumbleStatusRight = newRight;

        _rumbleStatusPlug = std::clamp(_rumbleStatusLeft + _rumbleStatusRight, 0, 100);
        _buttplugDevice.setVibrate(_rumbleStatusPlug);

        if (_physicalPadId != 0xFFFF) {
            XINPUT_VIBRATION vibration;
            vibration.wLeftMotorSpeed = (static_cast<WORD>(LargeMotor) << 8) | LargeMotor;
            vibration.wRightMotorSpeed = (static_cast<WORD>(SmallMotor) << 8) | SmallMotor;
            XInputSetState(_physicalPadId, &vibration);
        }
    }
}

void ViGem360Pad::rumbleCallback(UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber) {
    if (LedNumber != _virtualPadPlayerIndex) {
        printf("Rumble CB: got LEDD %d\n", LedNumber);
        _virtualPadPlayerIndex = LedNumber;
		System::SetEvent(_padIdAssigmentEvent);
    }
    ++_rumbleInstructionCount;
	setRumble(LargeMotor, SmallMotor);
}

bool ViGem360Pad::getRumbleState(int* commandCount, int* statusLeft, int* statusRight, int* statusPlug) {
    *statusLeft = _rumbleStatusLeft;
    *statusRight = _rumbleStatusRight;
    *statusPlug = _rumbleStatusPlug;
    if (*commandCount == _rumbleInstructionCount)
        return false;
    *commandCount = _rumbleInstructionCount;
    return true;
}

void ViGem360Pad::adjustRumble(int adjustLeft, int adjustRight) {
    _rumbleScaleLeft = std::clamp(_rumbleScaleLeft + adjustLeft, 0, MAX_RUMBLE);
    _rumbleScaleRight = std::clamp(_rumbleScaleRight + adjustRight, 0, MAX_RUMBLE);
    _config.setVibration(_rumbleScaleLeft, _rumbleScaleRight);
    _config.toFile();
}

void ViGem360Pad::getAnalogueAsByte(UCHAR* left, UCHAR* right) {
    int value;

    value = max(abs(_padState.Gamepad.sThumbLX), abs(_padState.Gamepad.sThumbLY));
    value -= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    value = (value * 255) / (32767 - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    *left = std::clamp(value, 0, 255);

    value = max(abs(_padState.Gamepad.sThumbRX), abs(_padState.Gamepad.sThumbRY));
    value -= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
    value = (value * 255) / (32767 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
	*right = std::clamp(value, 0, 255);
}

int ViGem360Pad::getFirstPhysicalControllerIndex() {
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        if (i == _virtualPadPlayerIndex)
            continue;

        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS)
            return i;
    }
    return -1;
}

int ViGem360Pad::waitForIdAssignment() {
	System::WaitEvent(_padIdAssigmentEvent);
	return (int)_virtualPadPlayerIndex;
}

int ViGem360Pad::getVirtualPadUserIndex() {
    return (int)_virtualPadPlayerIndex;
}

void ViGem360Pad::attachToPhysicalPad(DWORD physicalPadId) {
    _physicalPadId = physicalPadId;
}

VOID CALLBACK ViGem360Pad::rumbleCallbackFn(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData) {
	((ViGem360Pad*)UserData)->rumbleCallback(LargeMotor, SmallMotor, LedNumber);
}
