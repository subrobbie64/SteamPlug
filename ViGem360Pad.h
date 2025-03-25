#pragma once

#include "ButtplugDiscovery.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <ViGEm/Client.h>
#include "System.h"

class ButtplugDevice;
class ButtplugConfig;

class ViGem360Pad {
public:
    ViGem360Pad(ButtplugDevice& buttplugDevice, ButtplugConfig& config);
    ~ViGem360Pad();

    int waitForIdAssignment();

    int getVirtualPadUserIndex();
    int getFirstPhysicalControllerIndex();
    void attachToPhysicalPad(DWORD physicalPadId);

    void adjustRumble(int adjustLeft, int adjustRight);

    void updateState();
    bool getRumbleState(int* commandCount, int* statusLeft, int* statusRight, int* statusPlug);
	void getAnalogueAsByte(UCHAR* left, UCHAR* right);

    void setRumble(UCHAR LargeMotor, UCHAR SmallMotor);
    void rumbleCallback(UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber);
private:
    static VOID CALLBACK rumbleCallbackFn(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData);

    PVIGEM_CLIENT _client;
    PVIGEM_TARGET _outputPad;

    unsigned long _virtualPadPlayerIndex;
    DWORD _physicalPadId, _lastPacketId;

    ButtplugDevice& _buttplugDevice;
    ButtplugConfig& _config;

    int _rumbleInstructionCount, _rumbleStatusLeft, _rumbleStatusRight, _rumbleStatusPlug;
    int _rumbleScaleLeft, _rumbleScaleRight;

    XINPUT_STATE _padState;

    sysevent_t _padIdAssigmentEvent;
};
