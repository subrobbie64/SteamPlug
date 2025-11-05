#pragma once

#define VENDOR_ID_SONY 0x054C

#define DUALSHOCK4_PRODUCT_ID_USB 0x05C4
#define DUALSHOCK4_PRODUCT_ID_USB_V2 0x09CC
#define DUALSHOCK4_PRODUCT_ID_DONGLE 0x0BA0
#define DUALSHOCK4_PRODUCT_ID_BT 0x081F

#define DUALSENSE_PRODUCT_ID_USB 0x0CE6
#define DUALSENSE_PRODUCT_ID_USB_V2 0x0DF2

#define DS5_MUTE_LED 0
#define DS5_PLAYER_LEDS 0x00
#define DS5_LIGHT_COLOR 0x00FF00

#define DS_WIRELESS_BATTERY_MAX  8
#define DS_CHARGING_BATTERY_MAX 11

#define DS4_REPORT_USB       0x01
#define DS4_REPORT_BLUETOOTH 0x11

// buttonDpad
#define DS_TRIANGLE 0x80
#define DS_CIRCLE   0x40
#define DS_CROSS    0x20
#define DS_SQUARE   0x10
// moreButtons
#define DS_R3	   0x80
#define DS_L3	   0x40
#define DS_OPTIONS 0x20
#define DS_SHARE   0x10	// CREATE on DS5
#define DS_R1	   0x02
#define DS_L1	   0x01

struct DS4OutputState {
	unsigned char smallRumble;
	unsigned char bigRumble;
	unsigned char r;
	unsigned char g;
	unsigned char b;
};

struct DS4RawState {
	unsigned char leftX;
	unsigned char leftY;
	unsigned char rightX;
	unsigned char rightY;
	unsigned char buttonDpad;
	unsigned char moreButtons;
	unsigned char counterButtons;
	unsigned char leftTrigger;
	unsigned char rightTrigger;
	unsigned char timestamp[2];
	unsigned char battery;
	unsigned char gyroX[2];
	unsigned char gyroY[2];
	unsigned char gyroZ[2];
	unsigned char accelX[2];
	unsigned char accelY[2];
	unsigned char accelZ[2];
	unsigned char unknown[5];
	unsigned char extensionBattery;
	unsigned char unknown2[2];
	unsigned char touchpadEventFlags;
	unsigned char touchpadSequence;
	unsigned char touchpadFingerTracking1;
	unsigned char touchpadCoordinate1[3];
	unsigned char touchpadFingerTracking2;
	unsigned char touchpadCoordinate2[3];
	unsigned char touchpadPreviousFinger1Tracking;
	unsigned char touchpadPreviousFinger1Coordinate[3];
	unsigned char touchpadPreviousFinger2Tracking;
	unsigned char touchpadPreviousFinger2Coordinate[3];
	unsigned char unknown3[12];
};

struct DS5RawStateBluetooth01 { // Bluetooth Report 0x01
	unsigned char leftX;
	unsigned char leftY;
	unsigned char rightX;
	unsigned char rightY;
	unsigned char buttonDpad;
	unsigned char moreButtons;
	unsigned char buttons3;
	unsigned char leftTrigger;
	unsigned char rightTrigger;
};

struct DS5RawState { // USB Report 0x01 as well as Bluetooth Report 0x31
	unsigned char leftX;
	unsigned char leftY;
	unsigned char rightX;
	unsigned char rightY;
	unsigned char leftTrigger;
	unsigned char rightTrigger;
	unsigned char seqNum;
	unsigned char buttonDpad;
	unsigned char moreButtons;
	unsigned char buttons[2];
	unsigned char timestamp[4];
	unsigned char gyro[6];
	unsigned char accel[6];
	unsigned char sensorTimestamp[4];
	unsigned char unknown_post_timestamp;
	unsigned char touch[8];
	unsigned char unknown_post_touch;
	unsigned char r2Feedback;
	unsigned char l2Feedback;
	unsigned char unknown_post_fb[9];
	unsigned char battery[2];
};


