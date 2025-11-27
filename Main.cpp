#include <iostream>
#include <Windows.h>
#include "BluetoothDiscovery.h"
#include "ButtplugDevice.h"
#include "Hush2Device.h"
#include "HismithDevice.h"
#include "ButtplugConfig.h"
#include "XBoxPad.h"
#include "DualShockPad.h"
#include "System.h"
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
        LINE_KEY_HELP = 16,
        LINE_EVENT_STATUS = 17
    };

    ButtplugDevice* openButtplugDevice();
    PhysicalPad* openGamePad(unsigned long long* waitPadDetection, ControllerMode* mode, const VirtualPad& virtualPad, int* physicalPadIndex);

    void printButtplugStatus(int rumbleCommands, int rumbleLeft, int rumbleRight);
    void printTestStatus(PhysicalPad* physicalPad, VirtualPad& virtualPad);
    void printVibrationStatus();
    void printBar(Color col, int y, const char* prefix, int value);
    void printPlugBatteryLevel(unsigned char batteryLevel);
    void printPadBatteryLevel(unsigned char batteryLevel);

    bool keyToCoyoteAction(int key, int* adjustLeft, int* adjustRight);
    bool keyToAction(int key, int* adjustLeft, int* adjustRight);

    void atProgramExit();
    static void __cdecl atProgramExitFunc();

    ButtplugConfig* _buttplugConfig;
    ButtplugDevice* _buttplugDevice;

    int _terminalWidth, _terminalHeight;

    static SteamPlugMain* s_MainInstance;
};

SteamPlugMain* SteamPlugMain::s_MainInstance = NULL;

SteamPlugMain::SteamPlugMain() : _buttplugConfig(NULL), _buttplugDevice(NULL), _terminalWidth(0), _terminalHeight(0) {
    s_MainInstance = this;
    atexit(atProgramExitFunc);
}

SteamPlugMain::~SteamPlugMain() {
}

ButtplugDevice* SteamPlugMain::openButtplugDevice() {
    _buttplugConfig = ButtplugConfig::fromFile();
    if (!_buttplugConfig->isValid()) {
        log("No " DEVICE_NAME " address found in config, running Discovery!\n");        
#ifdef USE_HUSH2
        BluetoothDiscovery deviceDiscovery(BTD_HUSH2, "Hush2 Buttplug");
#elif USE_COYOTE
        BluetoothDiscovery deviceDiscovery(BTD_COYOTE3, "Coyote 3.0");
#else
        BluetoothDiscovery deviceDiscovery(BTD_HISMITH, "Hismith Fuck Machine");
#endif
        if (!deviceDiscovery.runDiscovery(_buttplugConfig))
            error("No device found!\n");
        _buttplugConfig->toFile();

        Sleep(2000);
        Terminal::clearScreen();
        Terminal::printXy(1, 2, WHITE, "");
    }
    
    ButtplugDevice* buttplugDevice;
    log("Trying to connect " DEVICE_NAME " at %s...", BluetoothBase::MacToString(_buttplugConfig->getAddress()).c_str());
#ifdef USE_HUSH2
    buttplugDevice = new HushDevice(*_buttplugConfig);
#elif USE_COYOTE
    buttplugDevice = new CoyoteDevice(*_buttplugConfig);
#else
    buttplugDevice = new HismithDevice(*_buttplugConfig);
#endif
    while (!buttplugDevice->isConnected()) {
        buttplugDevice->connect();
        System::Sleep(500);
    }
	log("%s connected.\n", buttplugDevice->getDeviceName().c_str());
    System::Sleep(500);

    buttplugDevice->setGamepadVibration(80, 80);
    Sleep(200);
    buttplugDevice->setGamepadVibration(25, 25);
    Sleep(200);
    buttplugDevice->setGamepadVibration(0, 0);

    Terminal::clearScreen();

    return buttplugDevice;
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

PhysicalPad* SteamPlugMain::openGamePad(unsigned long long* waitPadDetection, ControllerMode *mode, const VirtualPad& virtualPad, int *physicalPadIndex) {
    if (*waitPadDetection <= System::GetMicros()) {
        DualPad* dsPad = DualPad::detectController();
        if (dsPad) {
            *mode = DUALSHOCK;
            *physicalPadIndex = -1;
            return dsPad;
        }
        XBoxPad* xboxPad = XBoxPad::detectController(virtualPad.getVirtualPadUserIndex());
        if (xboxPad) {
            *mode = XBOXPAD;
            *physicalPadIndex = xboxPad->getPadId();
            return xboxPad;
        }
        *waitPadDetection = System::GetMicros() + DETECT_PAD_DELAY_MICROS;
    }
    return NULL;
}

void SteamPlugMain::run() {
    _buttplugDevice = openButtplugDevice();
    VirtualPad virtualPad(*_buttplugDevice);

    unsigned long long waitPadDetection = 0;
    int cycleCount = 0, lastPlugBatteryLevel = -1, lastPadBatteryLevel = -1;
    bool physPadWasPresent = true;
    bool testing = false;
    ControllerMode mode;
    int physicalPadIndex;
    PhysicalPad* physicalPad = NULL;
	Terminal::clearScreen();
    while (true) {
        Terminal::getTerminalSize(&_terminalWidth, &_terminalHeight);

        if (!_buttplugDevice->isConnected())
			_buttplugDevice->connect();

        virtualPad.updateState();

        if ((cycleCount % 5) == 0) {
            if (_kbhit() || (cycleCount == 0)) {
				int key = _kbhit() ? _getch() : 0;
                int adjustLeft, adjustRight;
                if (key == 't') {
                    testing = !testing;
                    Terminal::clearScreen();
                    cycleCount = 0;
                    virtualPad.setRumble(0, 0);
                    System::Sleep(50);
                } else if (keyToAction(key, &adjustLeft, &adjustRight))
                    _buttplugDevice->adjustVibration(adjustLeft, adjustRight);
#ifdef USE_COYOTE
				else if (keyToCoyoteAction(key, &adjustLeft, &adjustRight))
                    static_cast<CoyoteDevice*>(_buttplugDevice)->adjustChannelIntensity(adjustLeft, adjustRight);
#endif
                printVibrationStatus();
            }
        }

        if (testing)
            printTestStatus(physicalPad, virtualPad);

        bool physPadIsPresent = (physicalPad != NULL) && !physicalPad->isError();
        if ((cycleCount == 0) || !physicalPad || (physPadWasPresent != physPadIsPresent)) {
            if (!physicalPad || physicalPad->isError()) {
                Terminal::printXy(1, LINE_PHYSPAD_STATUS, RED, "Physical gamepad: waiting for controller.");
                virtualPad.setPhysicalPad(NULL);
                delete physicalPad;
                physicalPad = openGamePad(&waitPadDetection, &mode, virtualPad, &physicalPadIndex);
				if (physicalPad)
                    cycleCount = 0;
            } else {
				Terminal::clearLine(LINE_PHYSPAD_STATUS);
                Terminal::printXy(1, LINE_PHYSPAD_STATUS, GREEN, "Physical gamepad: %s ", (mode == DUALSHOCK) ? "DualShock" : "XBox");
                if (physicalPadIndex >= 0)
					printf("#%d     ", physicalPadIndex + 1);
                virtualPad.setPhysicalPad(physicalPad);
            }
            Terminal::printXy(1, LINE_VIRTPAD_STATUS, GREEN, "Emulating X360 controller #%d.", virtualPad.getVirtualPadUserIndex() + 1);
        }
        physPadWasPresent = physPadIsPresent;

        int rumbleCommands, rumbleLeft, rumbleRight;
        if (virtualPad.getRumbleState(&rumbleCommands, &rumbleLeft, &rumbleRight) || ((cycleCount % 100) == 0))
            printButtplugStatus(rumbleCommands, rumbleLeft, rumbleRight);

        if ((cycleCount == 0) || (lastPlugBatteryLevel != _buttplugDevice->getBatteryLevel())) {
            lastPlugBatteryLevel = _buttplugDevice->getBatteryLevel();
            printPlugBatteryLevel(lastPlugBatteryLevel);
        }
		if ((cycleCount == 0) || (physicalPad && (lastPadBatteryLevel != physicalPad->getBatteryState()))) {
            lastPadBatteryLevel = physicalPad ? physicalPad->getBatteryState() : 0;
            printPadBatteryLevel(lastPadBatteryLevel);
		}

        ++cycleCount;
        System::Sleep(10);
    }
}

void SteamPlugMain::printButtplugStatus(int rumbleCommands, int rumbleLeft, int rumbleRight) {
    if (_buttplugDevice->isConnected())
        Terminal::printXy(1, LINE_PLUG_STATUS, GREEN, DEVICE_NAME " connected.                     ");
    else
        Terminal::printXy(1, LINE_PLUG_STATUS, RED, DEVICE_NAME " disconnected, reconnecting...");

    int rumblePlug = _buttplugDevice->getEffectiveVibration();
    Terminal::printXy(1, LINE_RUMBLE_STATUS, WHITE, "Rumble instructions: %d\nStatus: L=%3d, R=%3d => Plug=%3d", rumbleCommands, rumbleLeft, rumbleRight, rumblePlug);
    Terminal::printXy(1, LINE_EVENT_STATUS, GREEN, "Processing events.");
}

void SteamPlugMain::printTestStatus(PhysicalPad* physicalPad, VirtualPad& virtualPad) {
    UCHAR left = 0, right = 0;
    if (physicalPad)
        physicalPad->getAnalogSticksAsByte(&left, &right);
    virtualPad.setRumble(left, right);
    int rumblePlug = _buttplugDevice->getEffectiveVibration();

    Terminal::printXy(0, LINE_TEST_STATUS, RED, "TESTING VIBRATIONS ONLY");
    printBar(YELLOW, LINE_TEST_STATUS + 1, "Large motor:", (100 * left) / 255);
    printBar(YELLOW, LINE_TEST_STATUS + 2, "Small motor:", (100 * right) / 255);
    printBar(RED, LINE_TEST_STATUS + 3, "Buttplug:   ", rumblePlug);
    Terminal::printXy(0, LINE_TEST_STATUS + 4, RED, "Use left and right analog sticks");
}

void SteamPlugMain::printVibrationStatus() {
#ifdef USE_COYOTE
    int coyoteChannelA, coyoteChannelB;
    static_cast<CoyoteDevice*>(_buttplugDevice)->getChannelIntensity(&coyoteChannelA, &coyoteChannelB);
    Terminal::printXy(_terminalWidth - 60, LINE_KEY_HELP - 2, WHITE, "Channel A Level: \x1B[%02Xm%3d%%\x1B[%02Xm   /   Channel B Level: \x1B[%02Xm%3d%%", YELLOW, coyoteChannelA, WHITE, YELLOW, coyoteChannelB);
    Terminal::printXy(_terminalWidth - 60, LINE_KEY_HELP - 1, WHITE, "Ch A: Q/A for up/down       Ch B: W/S for up/down");
#endif
    int rumbleScaleLeft, rumbleScaleRight;
    _buttplugDevice->getVibrationIntensity(&rumbleScaleLeft, &rumbleScaleRight);
    Terminal::printXy(_terminalWidth - 20, LINE_KEY_HELP + 0, WHITE, "Vib L/R: \x1B[%02Xm%3d%% / %3d%%", YELLOW, rumbleScaleLeft, rumbleScaleRight);
    Terminal::printXy(_terminalWidth - 20, LINE_KEY_HELP + 1, WHITE, "O/L: Left, +/-: Right");
    Terminal::printXy(_terminalWidth - 20, LINE_KEY_HELP + 2, WHITE, "T: Test Vibrations");
}

void SteamPlugMain::printBar(Color col, int y, const char *prefix, int value) {
    int barWidth = (int)(_terminalWidth - 7 - strlen(prefix));
    if (barWidth >= 0) {
        int barFill = (value * barWidth) / 100;
		char* barBuffer = (char*)_alloca(barWidth + 2);
        memset(barBuffer, ' ', barWidth + 1);
        memset(barBuffer, '=', barFill);
        barBuffer[barFill] = ']';
        barBuffer[barWidth + 1] = '\0';
        Terminal::printXy(1, y, col, "%s [%s%3d%%", prefix, barBuffer, value);
    }
}

void SteamPlugMain::printPlugBatteryLevel(unsigned char plugBatteryLevel) {
    Color col = GREEN;
    if (plugBatteryLevel <= PLUG_BATTERY_LEVEL_CRITICAL)
        col = RED; 
    else if (plugBatteryLevel <= PLUG_BATTERY_LEVEL_LOW)
        col = YELLOW;

    Terminal::clearLine(LINE_PLUG_BATTERY);
    if (plugBatteryLevel & BUTTPLUG_WIRED) {
        Terminal::printXy(1, LINE_PLUG_BATTERY, GREEN, DEVICE_NAME);
        Terminal::printXy(0, LINE_PLUG_BATTERY, GREEN, " -Wired- ");
    } else
        printBar(col, LINE_PLUG_BATTERY, DEVICE_NAME " bat:", plugBatteryLevel);
}

void SteamPlugMain::printPadBatteryLevel(unsigned char batteryLevel) {
    Terminal::clearLine(LINE_PHYSPAD_BATTERY);
    if (batteryLevel == BATTERY_WIRED) {
        Terminal::printXy(1, LINE_PHYSPAD_BATTERY, GREEN, "Pad bat:");
        Terminal::printXy(0, LINE_PHYSPAD_BATTERY, GREEN, " -Wired- ");
    } else {
        const bool charging = batteryLevel & BATTERY_CHARGING;
        batteryLevel &= ~BATTERY_CHARGING;
        Color col = GREEN;
        if (batteryLevel <= PLUG_BATTERY_LEVEL_CRITICAL)
            col = RED;
        else if (batteryLevel <= PLUG_BATTERY_LEVEL_LOW)
            col = YELLOW;
        printBar(col, LINE_PHYSPAD_BATTERY, "Pad bat: ", batteryLevel);
        if (charging)
            Terminal::printXy(0, LINE_PHYSPAD_BATTERY, GREEN, " -Charging- ");
    }
}

void SteamPlugMain::atProgramExit() {
    if (_buttplugDevice) {
        _buttplugDevice->setGamepadVibration(0, 0);
        System::Sleep(250);
    }
}

void __cdecl SteamPlugMain::atProgramExitFunc() {
    s_MainInstance->atProgramExit();
}

int main() {
    SetConsoleTitle("Buttplug for Steam X360 emulation");
    Terminal::enableVirtualTerminalMode();
    Terminal::setTerminalCursorVisibility(false);

    SteamPlugMain main;
	main.run();

	return 0;
}
