#include "VirtualPad.h"
#include "System.h"
#include <algorithm>

#pragma comment(lib, "ViGEmClient.lib")

#define OOT_HACK
#ifdef OOT_HACK
class EffectThread {
public:
	EffectThread(VirtualPad* virtualPad) : _virtualPad(virtualPad), _effectId(0) {
		_effectEvent = System::CreateEventFlag();
		_thread = System::CreateThread(&EffectThread::threadFunc, this);
	}
	~EffectThread() {
		System::WaitThread(_thread);
	}
	void fire(int id);
private:
	VirtualPad* _virtualPad;
	systhread_t _thread;
	sysevent_t _effectEvent;
	int _effectId;

	void thread();
	static threadReturn WINAPI threadFunc(void* arg);
};

void EffectThread::thread() {
	while (true) {
		System::WaitEvent(_effectEvent);
		_virtualPad->setRumble(255, 255);
		System::Sleep(250);
		_virtualPad->setRumble(0, 0);
		if (_effectId != 1)
			System::Sleep(200);
	}
}

void EffectThread::fire(int id) {
	_effectId = id;
	System::SetEvent(_effectEvent);
}

threadReturn WINAPI EffectThread::threadFunc(void* arg) {
	((EffectThread*)arg)->thread();
	return THREAD_RETURN;
}

static EffectThread* g_effectThread;
#endif

VirtualPad::VirtualPad(ButtplugDevice& buttplugDevice)
	: _client(vigem_alloc()), _outputPad(NULL), _virtualPadPlayerIndex(-1), _buttplugDevice(buttplugDevice), _physicalPad(NULL), _rumbleInstructionCount(0), _padState() {
	
	_rumbleStatusLarge = _rumbleStatusSmall = 0;

	if (_client == nullptr)
		error("vigem_alloc failed!\n");

	VIGEM_ERROR retval = vigem_connect(_client);
	if (!VIGEM_SUCCESS(retval))
		error("ViGEm Bus connection failed with error code: 0x%X\n", retval);

	_padIdAssigmentEvent = System::CreateEventFlag();

	debug("Setting up virtual x360 controller...");
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

	System::WaitEvent(_padIdAssigmentEvent);

	System::CreateSema(&_physicalPadSema, 1);
#ifdef OOT_HACK
	g_effectThread = new EffectThread(this);
#endif
}

VirtualPad::~VirtualPad() {
	vigem_target_remove(_client, _outputPad);
	vigem_target_free(_outputPad);

	vigem_disconnect(_client);
	vigem_free(_client);

	System::DestroyEventFlag(_padIdAssigmentEvent);
	System::DestroySema(&_physicalPadSema);
}

int VirtualPad::getVirtualPadUserIndex() const {
	return _virtualPadPlayerIndex;
}

void VirtualPad::setPhysicalPad(PhysicalPad* physicalPad) {
	System::WaitSema(&_physicalPadSema);
	_physicalPad = physicalPad;
	System::SignalSema(&_physicalPadSema);
}

bool VirtualPad::isPhysicalPadError() {
	bool isError = true;
	System::WaitSema(&_physicalPadSema);
	if (_physicalPad)
		isError = _physicalPad->isError();
	System::SignalSema(&_physicalPadSema);
	return isError;
}

void VirtualPad::updateState() {
	static bool waitRelease = false;
	System::WaitSema(&_physicalPadSema);
	bool gotUpdate = false, isError = false;
	if (_physicalPad) {
		gotUpdate = _physicalPad->getState(&_padState);
		isError = _physicalPad->isError();
	} else
		isError = true;
	System::SignalSema(&_physicalPadSema);
	if (gotUpdate || isError) {
		if (isError)
			memset(&_padState, 0, sizeof(XUSB_REPORT));
#ifdef OOT_HACK
		if (_padState.wButtons & XINPUT_GAMEPAD_START)
			g_effectThread->fire(0);
		else if ((_padState.bLeftTrigger >= 0x30) || (_padState.bRightTrigger >= 0x30)) {
			if (!waitRelease)
				g_effectThread->fire(1);
			waitRelease = true;
		} else
			waitRelease = false;
#endif
		vigem_target_x360_update(_client, _outputPad, _padState);
	}
}

void VirtualPad::getAnalogSticksAsByte(UCHAR* left, UCHAR* right) const {
	int value;

	value = max(abs(_padState.sThumbLX), abs(_padState.sThumbLY));
	value -= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
	value = (value * 255) / (32767 - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	*left = std::clamp(value, 0, 255);

	value = max(abs(_padState.sThumbRX), abs(_padState.sThumbRY));
	value -= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
	value = (value * 255) / (32767 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
	*right = std::clamp(value, 0, 255);
}

void VirtualPad::setRumble(UCHAR LargeMotor, UCHAR SmallMotor) {
	if ((_rumbleStatusLarge != LargeMotor) || (_rumbleStatusSmall != SmallMotor)) {
		_rumbleStatusLarge = LargeMotor;
		_rumbleStatusSmall = SmallMotor;
		_buttplugDevice.setVibrate(SmallMotor, LargeMotor);

		System::WaitSema(&_physicalPadSema);
		if (_physicalPad)
			_physicalPad->updatePadRumble(LargeMotor, SmallMotor);
		System::SignalSema(&_physicalPadSema);
	}
}

bool VirtualPad::getRumbleState(int* commandCount, int* statusLarge, int* statusSmall) const {
	*statusLarge = _rumbleStatusLarge;
	*statusSmall = _rumbleStatusSmall;
	if (*commandCount == _rumbleInstructionCount)
		return false;
	*commandCount = _rumbleInstructionCount;
	return true;
}

void VirtualPad::rumbleCallback(UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber) {
	if (_virtualPadPlayerIndex != LedNumber) {
		_virtualPadPlayerIndex = LedNumber;
		System::SetEvent(_padIdAssigmentEvent);
	}
	++_rumbleInstructionCount;
	setRumble(LargeMotor, SmallMotor);
}

VOID CALLBACK VirtualPad::rumbleCallbackFn(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData) {
	((VirtualPad*)UserData)->rumbleCallback(LargeMotor, SmallMotor, LedNumber);
}
