#pragma once

#include "ButtplugDevice.h"
#include "ButtplugConfig.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <ViGEm/Client.h>
#include "System.h"

#define BATTERY_WIRED 0xFF
#define BATTERY_CHARGING 0x80
#define BATTERY_MAX 100

class PhysicalPad {
public:
	virtual bool getState(XUSB_REPORT* padReport) = 0;
	virtual unsigned char getBatteryState() = 0;
	virtual void updatePadRumble(UCHAR LargeMotor, UCHAR SmallMotor) = 0;

	virtual bool isError() const = 0;
private:
};

class VirtualPad {
public:
	VirtualPad(AbstractButtDevice& buttplugDevice);
	~VirtualPad();

	int getVirtualPadUserIndex() const;

	void setPhysicalPad(PhysicalPad* physicalPad);
	bool isPhysicalPadError();

	void updateState();
	void getAnalogSticksAsByte(UCHAR* left, UCHAR* right) const;
	
	bool getRumbleState(int* commandCount, int* statusLarge, int* statusSmall) const;
	void setRumble(UCHAR LargeMotor, UCHAR SmallMotor);

private:
	void rumbleCallback(UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber);
	static VOID CALLBACK rumbleCallbackFn(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData);

	PVIGEM_CLIENT _client;
	PVIGEM_TARGET _outputPad;

	AbstractButtDevice& _buttplugDevice;

	PhysicalPad* _physicalPad;
	XUSB_REPORT _padState;

	int _rumbleInstructionCount, _rumbleStatusSmall, _rumbleStatusLarge;

	unsigned long _virtualPadPlayerIndex;

	sysevent_t _padIdAssigmentEvent;
	syssema_t _physicalPadSema;
};
