#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <ViGEm/Client.h>

class PhysicalPad {
public:
	PhysicalPad();
	virtual ~PhysicalPad();
	virtual bool getState(XUSB_REPORT* padReport) = 0;
	virtual unsigned char getBatteryState() = 0;
	virtual void updatePadRumble(UCHAR LargeMotor, UCHAR SmallMotor) = 0;
	void getAnalogSticksAsByte(UCHAR* left, UCHAR* right) const;

	virtual bool isError() const = 0;
protected:
	XINPUT_STATE _padState;
};

