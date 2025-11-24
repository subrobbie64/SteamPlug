#pragma once

#include "wclBluetooth.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "PhysicalPad.h"

class XBoxPad : public PhysicalPad {
public:
    XBoxPad(DWORD physicalPadId);
    virtual ~XBoxPad();

    DWORD getPadId() const;

    virtual bool isError() const;

    virtual bool getState(XUSB_REPORT* padReport);
	virtual unsigned char getBatteryState();
    virtual void updatePadRumble(UCHAR LargeMotor, UCHAR SmallMotor);

    static XBoxPad *detectController(int virtualPadId);
private:
    DWORD _physicalPadId;

    bool _deviceError;
};
