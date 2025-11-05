#pragma once

#include "ButtplugDiscovery.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include "System.h"
#include "VirtualPad.h"

class XBoxPad : public PhysicalPad {
public:
    XBoxPad(DWORD physicalPadId);
    ~XBoxPad();

    DWORD getPadId();

    virtual bool isError() const;

    virtual bool getState(XUSB_REPORT* padReport);
	virtual unsigned char getBatteryState();
    virtual void updatePadRumble(UCHAR LargeMotor, UCHAR SmallMotor);

    static XBoxPad *detectController(int virtualPadId);
private:
    DWORD _physicalPadId;

    XINPUT_STATE _padState;
    bool _deviceError;
};
