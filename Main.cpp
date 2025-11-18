#include <iostream>
#include <algorithm>
#include <Windows.h>
#include "ButtplugDiscovery.h"
#include "ButtplugDevice.h"
#include "Hush2Device.h"
#include "Hush2Discovery.h"
#include "HismithDevice.h"
#include "HismithDiscovery.h"
#include "ButtplugConfig.h"
#include "XBoxPad.h"
#include "DualShockPad.h"
#include "System.h"
#include "Coyote3Discovery.h"
#include "Coyote3Device.h"
#include <conio.h>
#include "resource.h"

#pragma warning(disable: 6255)

#ifdef USE_HUSH2
#define DEVICE_NAME "Buttplug"
#elif USE_COYOTE
#define DEVICE_NAME "Coyote"
#else
#define DEVICE_NAME "Hismith fuck machine"
#endif

#define PLUG_BATTERY_LEVEL_LOW 20
#define PLUG_BATTERY_LEVEL_CRITICAL 10
#define RUMBLE_STEP 2
#define DETECT_PAD_DELAY_MICROS 2500000

class SteamPlugMain {
public:
    enum ControllerMode {
        NONE,
        XBOXPAD,
        DUALSHOCK
    };
    SteamPlugMain();
    ~SteamPlugMain();

    void run();

private:
    enum TerminalLine {
        LINE_PLUG_STATUS = 1,
        LINE_PLUG_BATTERY = 2,
        LINE_PHYSPAD_STATUS = 3,
        LINE_PHYSPAD_BATTERY = 4,
        LINE_VIRTPAD_STATUS = 5,
        LINE_TEST_STATUS = 8,
        LINE_RUMBLE_STATUS = 13,
        LINE_KEY_HELP = 14,
        LINE_EVENT_STATUS = 17
    };

    void openButtplugDevice();
    PhysicalPad* openGamePad(ControllerMode* mode, int* physicalPadIndex, bool initialize);

    void printBar(Color col, int y, const char* prefix, int value);
    void printPlugBatteryLevel(unsigned char batteryLevel);
    void printPadBatteryLevel(unsigned char batteryLevel);

    bool keyToCoyoteAction(int key, int* adjustLeft, int* adjustRight);
    bool keyToAction(int key, int* adjustLeft, int* adjustRight);

    void atProgramExit();
    static void __cdecl atProgramExitFunc();

    ButtplugConfig* _buttplugConfig;
    ButtplugDevice* _buttplugDevice;
    CoyoteDevice* _coyoteDevice;

    VirtualPad* _virtualPad;

    unsigned long long _waitPadDetection;

    static SteamPlugMain* s_MainInstance;
};

SteamPlugMain* SteamPlugMain::s_MainInstance = NULL;

SteamPlugMain::SteamPlugMain() : _buttplugConfig(NULL), _buttplugDevice(NULL), _coyoteDevice(NULL), _virtualPad(NULL) {
    s_MainInstance = this;
    atexit(atProgramExitFunc);
    
    _waitPadDetection = 0;
}

SteamPlugMain::~SteamPlugMain() {
}

void SteamPlugMain::openButtplugDevice() {
    _buttplugConfig = ButtplugConfig::fromFile();
	ButtplugDiscovery* deviceDiscovery = NULL;
#ifdef USE_HUSH2
    if (!_buttplugConfig->getAddress()) {
        log("No Hush2 Buttplug address found in config, running Discovery!\n");
        deviceDiscovery = new HushDiscovery();
    }
#elif USE_COYOTE
    if (!_buttplugConfig->getAddress()) {
        log("No Coyote 3.0 address found in config, running Discovery!\n");
		deviceDiscovery = new CoyoteDiscovery();
    }
#else
    if (!_buttplugConfig->getAddress()) {
        log("No Hismith address found in config, running Discovery!\n");
        deviceDiscovery = new HismithDiscovery();
    }
#endif
    if (deviceDiscovery) {
        if (!deviceDiscovery->runDiscovery(_buttplugConfig))
            error("No device found!\n");
        delete deviceDiscovery;
        _buttplugConfig->toFile();

        Sleep(3000);
        clearScreen();
        printXy(1, 2, WHITE, "");
    }

#ifdef USE_HUSH2
    log("Trying to connect Buttplug at %s...", Mac2String(_buttplugConfig->getAddress()).c_str());
    _buttplugDevice = new HushDevice(*_buttplugConfig);
#elif USE_COYOTE
    log("Trying to connect Coyote 3.0 at %s...", Mac2String(_buttplugConfig->getAddress()).c_str());
    _coyoteDevice = new CoyoteDevice(*_buttplugConfig);
    _buttplugDevice = _coyoteDevice;
#else
    log("Trying to connect Hismith at %s...", Mac2String(_buttplugConfig->getAddress()).c_str());
    _buttplugDevice = new HismithDevice(*_buttplugConfig);
#endif
    _buttplugDevice->connect();
    while (!_buttplugDevice->isConnected())
        System::Sleep(1000);
	log("%s connected.\n", _buttplugDevice->getDeviceName().c_str());
    System::Sleep(500);

    _buttplugDevice->setVibrate(40, 40);
    Sleep(200);
    _buttplugDevice->setVibrate(20, 20);
    Sleep(200);
    _buttplugDevice->setVibrate(0, 0);

    clearScreen();
}

bool SteamPlugMain::keyToAction(int key, int* adjustLeft, int* adjustRight) {
	*adjustLeft = *adjustRight = 0;
    if (key == 'o')
        *adjustLeft = RUMBLE_STEP;
    else if (key == 'l')
        *adjustLeft = -RUMBLE_STEP;
    else if (key == '+')
        *adjustRight = RUMBLE_STEP;
    else if (key == '-')
        *adjustRight = -RUMBLE_STEP;
    else
        return false;

    return true;
}

bool SteamPlugMain::keyToCoyoteAction(int key, int* adjustLeft, int* adjustRight) {
    *adjustLeft = *adjustRight = 0;
    if (key == 'q')
        *adjustLeft = RUMBLE_STEP;
    else if (key == 'a')
        *adjustLeft = -RUMBLE_STEP;
    else if (key == 'w')
        *adjustRight = RUMBLE_STEP;
    else if (key == 's')
        *adjustRight = -RUMBLE_STEP;
    else
        return false;

    return true;
}

PhysicalPad* SteamPlugMain::openGamePad(ControllerMode *mode, int *physicalPadIndex, bool initialize) {
    if (_waitPadDetection <= System::GetMicros()) {
        DualPad* dsPad = DualPad::detectController();
        if (dsPad) {
            *mode = DUALSHOCK;
            *physicalPadIndex = -1;
            return dsPad;
        }
        XBoxPad* xboxPad = XBoxPad::detectController(_virtualPad->getVirtualPadUserIndex());
        if (xboxPad) {
            *mode = XBOXPAD;
            *physicalPadIndex = xboxPad->getPadId();
            return xboxPad;
        }
        _waitPadDetection = System::GetMicros() + DETECT_PAD_DELAY_MICROS;
    }
    return NULL;
}

void SteamPlugMain::run() {
    openButtplugDevice();
	_virtualPad = new VirtualPad(*_buttplugDevice);

    int cycleCount = 0, lastPlugBatteryLevel = -1, lastPadBatteryLevel = -1;
    int rumbleCommands = 0, rumbleLeft = 0, rumbleRight = 0, rumblePlug = 0;
    bool testing = false, wasTesting = false;
    ControllerMode mode;
    int physicalPadIndex;
    PhysicalPad* physicalPad = NULL;
    while (true) {
        if ((cycleCount % 100) == 0) {
            if (_buttplugDevice->isConnected()) {
                printXy(1, LINE_PLUG_STATUS, GREEN, DEVICE_NAME " connected.                   ");
            } else
                printXy(1, LINE_PLUG_STATUS, RED, DEVICE_NAME " disconnected, reconnecting...      ");
        }

        _virtualPad->updateState();
		if ((physicalPad == NULL) || physicalPad->isError()) {
            printXy(1, LINE_PHYSPAD_STATUS, WHITE, "Physical gamepad: waiting for controller.");
            _virtualPad->setPhysicalPad(NULL);
            delete physicalPad;
            physicalPad = NULL;
            physicalPad = openGamePad(&mode, &physicalPadIndex, cycleCount == 0);
            cycleCount = 0;
		}

        if (cycleCount == 0) {
            if (physicalPad) {
                char strBuf[8];
                printXy(1, LINE_PHYSPAD_STATUS, GREEN, "Physical gamepad: %s %s                      ", (mode == DUALSHOCK) ? "DualShock" : "XBox", (physicalPadIndex < 0) ? "" : _itoa(physicalPadIndex + 1, strBuf, 10));
                _virtualPad->setPhysicalPad(physicalPad);
            }
            printXy(1, LINE_VIRTPAD_STATUS, GREEN, "Emulating X360 controller with index %d.", _virtualPad->getVirtualPadUserIndex() + 1);
        }
        
        if (wasTesting != testing) {
            wasTesting = testing;
            clearScreen();
            cycleCount = 0;
            rumbleCommands = 0;
            System::Sleep(50);
			_virtualPad->setRumble(0, 0);
            System::Sleep(50);
        }

        if (testing) {
            UCHAR left, right;
            _virtualPad->getAnalogSticksAsByte(&left, &right);
            _virtualPad->setRumble(left, right);
            _virtualPad->getRumbleState(&rumbleCommands, &rumbleLeft, &rumbleRight);
            rumblePlug = _buttplugDevice->getEffectiveVibration();

            printXy(0, LINE_TEST_STATUS, RED, "TESTING VIBRATIONS ONLY");
            printBar(YELLOW, LINE_TEST_STATUS + 1, "Large motor:", (100 * left) / 255);
			printBar(YELLOW, LINE_TEST_STATUS + 2, "Small motor:", (100 * right) / 255);
            printBar(RED,    LINE_TEST_STATUS + 3, "Buttplug:   ", rumblePlug);
            printXy(0, LINE_TEST_STATUS + 4, RED, "Use left and right analog sticks");
        }
        if (_virtualPad->getRumbleState(&rumbleCommands, &rumbleLeft, &rumbleRight) || (cycleCount == 0)) {
            rumblePlug = _buttplugDevice->getEffectiveVibration();
            printXy(1, LINE_RUMBLE_STATUS, WHITE, "Rumble instructions: %d\nStatus: L=%3d, R=%3d => Plug=%3d", rumbleCommands, rumbleLeft, rumbleRight, rumblePlug);
            printXy(1, LINE_EVENT_STATUS, GREEN, "Processing events.");
        }

        if ((cycleCount % 5) == 0) {
            if (_kbhit() || (cycleCount == 0)) {
				int key = _kbhit() ? _getch() : 0;
                int adjustLeft, adjustRight;
                if (key == 't')
                    testing = !testing;
                if (keyToAction(key, &adjustLeft, &adjustRight))
                    _buttplugDevice->adjustVibration(adjustLeft, adjustRight);
#ifdef USE_COYOTE
				else if (keyToCoyoteAction(key, &adjustLeft, &adjustRight))
                    _coyoteDevice->adjustChannelIntensity(adjustLeft, adjustRight);
#endif

                int cols, rows;
                getTerminalSize(&cols, &rows);

                int rumbleScaleLeft, rumbleScaleRight;
                _buttplugDevice->getVibrate(&rumbleScaleLeft, &rumbleScaleRight);
                printXy(cols - 20, LINE_KEY_HELP + 0, WHITE, "Vib L/R: \x1B[%02Xm%3d%% / %3d%%", YELLOW, rumbleScaleLeft, rumbleScaleRight);
                printXy(cols - 20, LINE_KEY_HELP + 1, WHITE, "O/L: Left, +/-: Right");
                printXy(cols - 20, LINE_KEY_HELP + 2, WHITE, "T: Test Vibrations");
#ifdef USE_COYOTE
                int coyoteChannelA, coyoteChannelB;
				_coyoteDevice->getConfigVibrate(&coyoteChannelA, &coyoteChannelB);
                printXy(cols - 40, LINE_KEY_HELP + 0, WHITE, "Ch A/B: \x1B[%02Xm%3d%% / %3d%%", YELLOW, coyoteChannelA, coyoteChannelB);
                printXy(cols - 40, LINE_KEY_HELP + 1, WHITE, "Q/A: Ch A, W/S: Ch B");
#endif
            }
        }

        if (lastPlugBatteryLevel != _buttplugDevice->getBatteryLevel()) {
            lastPlugBatteryLevel = _buttplugDevice->getBatteryLevel();
            printPlugBatteryLevel(lastPlugBatteryLevel);
        }
		if ((physicalPad == NULL) || (lastPadBatteryLevel != physicalPad->getBatteryState())) {
            lastPadBatteryLevel = physicalPad ? physicalPad->getBatteryState() : 0;
            printPadBatteryLevel(lastPadBatteryLevel);
		}

        ++cycleCount;
        System::Sleep(10);
    }
    delete _virtualPad;
    _virtualPad = NULL;
}

void SteamPlugMain::printBar(Color col, int y, const char *prefix, int value) {
    int terminalWidth, rows;
    getTerminalSize(&terminalWidth, &rows);
    
    int barWidth = (int)(terminalWidth - 7 - strlen(prefix));
    if (barWidth >= 0) {
        int barFill = (value * barWidth) / 100;
		char* barBuffer = (char*)_alloca(barWidth + 2);
        memset(barBuffer, ' ', barWidth + 1);
        memset(barBuffer, '=', barFill);
        barBuffer[barFill] = ']';
        barBuffer[barWidth + 1] = '\0';
        printXy(1, y, col, "%s [%s%3d%%", prefix, barBuffer, value);
    }
}

void SteamPlugMain::printPlugBatteryLevel(unsigned char plugBatteryLevel) {
    Color col = GREEN;
    if (plugBatteryLevel <= PLUG_BATTERY_LEVEL_CRITICAL)
        col = RED; 
    else if (plugBatteryLevel <= PLUG_BATTERY_LEVEL_LOW)
        col = YELLOW;

    if (plugBatteryLevel & 0x80) {
        printXy(1, LINE_PLUG_BATTERY, GREEN, DEVICE_NAME);
        printXy(0, LINE_PLUG_BATTERY, GREEN, " -Wired- ");
    } else
	    printBar(col, LINE_PLUG_BATTERY, DEVICE_NAME " bat:", plugBatteryLevel);
}

void SteamPlugMain::printPadBatteryLevel(unsigned char batteryLevel) {
    if (batteryLevel == 0xFF) {
		printXy(1, LINE_PHYSPAD_BATTERY, GREEN, "Pad bat:");
        printXy(0, LINE_PHYSPAD_BATTERY, GREEN, " -Wired- ");
    } else {
        const bool charging = batteryLevel & 0x80;
        batteryLevel &= 0x7F;
        Color col = GREEN;
        if (batteryLevel <= PLUG_BATTERY_LEVEL_CRITICAL)
            col = RED;
        else if (batteryLevel <= PLUG_BATTERY_LEVEL_LOW)
            col = YELLOW;
        printBar(col, LINE_PHYSPAD_BATTERY, "Pad bat: ", batteryLevel);
        if (charging)
            printXy(0, LINE_PHYSPAD_BATTERY, GREEN, " -Charging- ");
    }
}

void SteamPlugMain::atProgramExit() {
	if (_virtualPad != NULL) {
		delete _virtualPad;
		_virtualPad = NULL;
	}
    if (_buttplugDevice) {
        _buttplugDevice->setVibrate(0, 0);
        System::Sleep(250);
    }
}

void __cdecl SteamPlugMain::atProgramExitFunc() {
    s_MainInstance->atProgramExit();
}

int main() {
    SetConsoleTitle("Buttplug for Steam X360 emulation");
    enableVirtualTerminalMode();
    setTerminalCursorVisibility(false);

    SteamPlugMain main;
	main.run();

	return 0;
}
