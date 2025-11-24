#include "PhysicalPad.h"
#include <algorithm>

PhysicalPad::PhysicalPad() : _padState() {
	memset(&_padState, 0, sizeof(XINPUT_STATE));
}

PhysicalPad::~PhysicalPad() {
}

void PhysicalPad::getAnalogSticksAsByte(UCHAR* left, UCHAR* right) const {
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
