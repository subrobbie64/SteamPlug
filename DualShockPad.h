#pragma once

#include "VirtualPad.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ViGEm/Client.h>
#include <hidapi.h>
#include "System.h"

#define HID_BUFFER_SIZE 1024

class DualPad : public PhysicalPad {
public:
	enum DeviceType {
		DUALSHOCK4 = 2,
		DUALSENSE  = 4
	};
	DualPad(unsigned short vendorId, unsigned short productId, const wchar_t *serial);
	virtual ~DualPad();

	virtual bool isError() const;

	virtual bool getState(XUSB_REPORT* padReport);
	virtual unsigned char getBatteryState();
	virtual void updatePadRumble(UCHAR LargeMotor, UCHAR SmallMotor);

	static DualPad *detectController();
protected:
	virtual void setRumbleColor(unsigned char largeRumble, unsigned char smallRumble, unsigned int color) = 0;
	virtual void convertPadState() = 0;

	int readHidWithRetries(unsigned char* buffer, int length);

	syssema_t _deviceSema;
	hid_device* _hidDevice;
	bool _isBluetooth;
	unsigned char _batteryState;
	bool _deviceError;

	unsigned char _inputBuffer[HID_BUFFER_SIZE];

	static const std::set<unsigned short> DUALSHOCK4_PRODUCT_IDS, DUALSENSE_PRODUCT_IDS;
};

class DualShockPad : public DualPad {
public:
	DualShockPad(unsigned short vendorId, unsigned short productId, const wchar_t* serial);
protected:
	virtual void setRumbleColor(unsigned char largeRumble, unsigned char smallRumble, unsigned int color);
	virtual void convertPadState();
};

class DualSensePad : public DualPad {
public:
	DualSensePad(unsigned short vendorId, unsigned short productId, const wchar_t* serial);
protected:
	virtual void setRumbleColor(unsigned char largeRumble, unsigned char smallRumble, unsigned int color);
	virtual void convertPadState();
private:
	void convertPadStateUsb01();
	void convertPadStateBluetooth01();

	unsigned char _outputSeq;
};
