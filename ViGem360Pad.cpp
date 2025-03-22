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
    : _client(vigem_alloc()), _outputPad(NULL), _physicalPadId(0xFFFF), _lastPacketId(0), _buttplugDevice(buttplugDevice), _config(config), _rumbleInstructionCount(0) {

	_config.getVibration(&_rumbleScaleLeft, &_rumbleScaleRight);
    _rumbleStatusLeft = _rumbleStatusRight = _rumbleStatusPlug = 0;

    _client = vigem_alloc();
    if (_client == nullptr)
        error("vigem_alloc failed!\n");
     
    VIGEM_ERROR retval = vigem_connect(_client);
    if (!VIGEM_SUCCESS(retval))
		error("ViGEm Bus connection failed with error code: 0x%X\n", retval);
    
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

	retval = vigem_target_x360_get_user_index(_client, _outputPad, &_virtualPadPlayerIndex);
	if (!VIGEM_SUCCESS(retval))
		error("Unable to get virtual controller index: 0x%X", retval);

    retval = vigem_target_x360_register_notification(_client, _outputPad, &rumbleCallbackFn, this);
    if (!VIGEM_SUCCESS(retval))
        error("Registering for notification failed with error code: 0x%X", retval);
}

ViGem360Pad::~ViGem360Pad() {
	vigem_target_remove(_client, _outputPad);
	vigem_target_free(_outputPad);

    vigem_disconnect(_client);
    vigem_free(_client);
}

void ViGem360Pad::updateState() {
    XINPUT_STATE state;
    // Grab the input from a physical X360 pad in this example
    if (XInputGetState(_physicalPadId, &state) != ERROR_SUCCESS)
        error("Error reading from X360 pad %d!", _physicalPadId + 1);

    if (_lastPacketId != state.dwPacketNumber) {
        _lastPacketId = state.dwPacketNumber;
        // The XINPUT_GAMEPAD structure is identical to the XUSB_REPORT structure
        // so we can simply take it "as-is" and cast it.
        //
        // Call this function on every input state change e.g. in a loop polling
        // another joystick or network device or thermometer or... you get the idea.
        vigem_target_x360_update(_client, _outputPad, *reinterpret_cast<const XUSB_REPORT*>(&state.Gamepad));
    }
}

void ViGem360Pad::rumbleCallback(UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber) {
    _rumbleStatusLeft = (_rumbleScaleLeft * LargeMotor) / 255;
    _rumbleStatusRight = (_rumbleScaleRight * SmallMotor) / 255;
    _rumbleStatusPlug = std::clamp(_rumbleStatusLeft + _rumbleStatusRight, 0, 100);
    _buttplugDevice.setVibrate(_rumbleStatusPlug);
    ++_rumbleInstructionCount;

    if (_physicalPadId != 0xFFFF) {
        XINPUT_VIBRATION vibration;
        vibration.wLeftMotorSpeed = (static_cast<WORD>(LargeMotor) << 8) | LargeMotor;
        vibration.wRightMotorSpeed = (static_cast<WORD>(SmallMotor) << 8) | SmallMotor;
        XInputSetState(_physicalPadId, &vibration);
    }
}

bool ViGem360Pad::getRumbleState(int* commandCount, int* statusLeft, int* statusRight, int* statusPlug) {
    if (*commandCount == _rumbleInstructionCount)
        return false;
    *commandCount = _rumbleInstructionCount;
    *statusLeft = _rumbleStatusLeft;
    *statusRight = _rumbleStatusRight;
    *statusPlug = _rumbleStatusPlug;
    return true;
}

void ViGem360Pad::adjustRumble(int adjustLeft, int adjustRight) {
    _rumbleScaleLeft = std::clamp(_rumbleScaleLeft + adjustLeft, 0, MAX_RUMBLE);
    _rumbleScaleRight = std::clamp(_rumbleScaleRight + adjustRight, 0, MAX_RUMBLE);
    _config.setVibration(_rumbleScaleLeft, _rumbleScaleRight);
    _config.toFile();
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

int ViGem360Pad::getVirtualPadUserIndex() {
    return (int)_virtualPadPlayerIndex;
}

void ViGem360Pad::attachToPhysicalPad(DWORD physicalPadId) {
    _physicalPadId = physicalPadId;
}

VOID CALLBACK ViGem360Pad::rumbleCallbackFn(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData) {
	((ViGem360Pad*)UserData)->rumbleCallback(LargeMotor, SmallMotor, LedNumber);
}
