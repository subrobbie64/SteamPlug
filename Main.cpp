#include <iostream>
#include <algorithm>
#include "ButtplugDiscovery.h"
#include "ButtplugDevice.h"
#include "ButtplugConfig.h"
#include "ViGem360Pad.h"
#include "System.h"

#include <conio.h>
#include "resource.h"

#define RUMBLE_STEP 2

class SteamPlugMain : public BatteryListener {
public:
    SteamPlugMain();

    void run();

    virtual void onBatteryLevelReceived(int batteryLevel);
private:
    enum TerminalLine {
        LINE_BAT_STATUS = 1,
        LINE_STATUS_CTRL = 7,
        LINE_VIRTUAL_CTRL = 3,
        LINE_PHYSICAL_CTRL = 4,
        LINE_RUMBLE_STATUS = 12,
        LINE_KEY_HELP = 13,
        LINE_PLUG_STATUS = 15,
        LINE_EVENT_STATUS = 16
    };

    void openButtplugDevice();

    void printBar(Color col, int y, const char* prefix, int value);
    void printBatteryLevel();

    void atProgramExit();
    static void __cdecl atProgramExitFunc();

    ButtplugConfig* _buttplugConfig;
    ButtplugDevice* _buttplugDevice;
    ViGem360Pad* _viGem360Pad;

    int _currentBatteryLevel;

    static SteamPlugMain* s_MainInstance;
};

SteamPlugMain* SteamPlugMain::s_MainInstance = NULL;

SteamPlugMain::SteamPlugMain() : _buttplugConfig(NULL), _buttplugDevice(NULL), _viGem360Pad(NULL), _currentBatteryLevel(-1) {
    s_MainInstance = this;
    atexit(atProgramExitFunc);
}

void SteamPlugMain::openButtplugDevice() {
    _buttplugConfig = ButtplugConfig::fromFile();
    if (_buttplugConfig == NULL) {
        log("No Buttplug address found in config, running Discovery!\n");

        ButtplugDiscovery discovery;
        if (!discovery.runDiscovery())
            error("No Buttplug device found!\n");

        _buttplugConfig = discovery.getAsConfiguration();
        _buttplugConfig->toFile();
        Sleep(3000);
        clearScreen();
        printXy(1, 2, WHITE, "");
    }

    _buttplugDevice = new ButtplugDevice(_buttplugConfig->getMacAddress(), _buttplugConfig->getButtplugDefinition());
    _buttplugDevice->setBatteryListener(this);
    log("Trying to connect Buttplug %s...", Mac2String(_buttplugConfig->getMacAddress()).c_str());
    _buttplugDevice->connect();
    _buttplugDevice->waitForConnection();
	log("connected.\n");

    _buttplugDevice->setVibrate(10);
    Sleep(200);
    _buttplugDevice->setVibrate(5);
    Sleep(200);
    _buttplugDevice->setVibrate(0);
}

int getFirstPhysicalControllerIndex(int virtualPadPlayerIndex = -1) {
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        if (i == virtualPadPlayerIndex)
            continue;

        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS)
            return i;
    }
    return -1;
}

void SteamPlugMain::run() {
    openButtplugDevice();
    printXy(1, LINE_PLUG_STATUS, GREEN, "Buttplug connected.\n");

    int usePhysicalPad = getFirstPhysicalControllerIndex();

    _viGem360Pad = new ViGem360Pad(*_buttplugDevice, *_buttplugConfig);
    printXy(1, LINE_STATUS_CTRL, WHITE, "Installed X360 controller");
    
    int virtualPadIndex = _viGem360Pad->getVirtualPadUserIndex();
    printXy(1, LINE_VIRTUAL_CTRL, GREEN, "Virtual controller index %d.", virtualPadIndex + 1);

    printXy(1, LINE_PHYSICAL_CTRL, YELLOW, "Waiting for physical controller...");
    while (usePhysicalPad < 0) {
        Sleep(1000);
        usePhysicalPad = _viGem360Pad->getFirstPhysicalControllerIndex();
    }

    printXy(1, LINE_PHYSICAL_CTRL, WHITE, "Attached to \x1B[%02Xmphysical controller %d!", GREEN, usePhysicalPad + 1);
    _viGem360Pad->attachToPhysicalPad(usePhysicalPad);

    int cols, rows;
    getTerminalSize(&cols, &rows);

    int cycleCount = -1;
    int rumbleCommands = 0, rumbleLeft = 0, rumbleRight = 0, rumblePlug = 0;
    int lastBatteryLevel = _currentBatteryLevel;
    bool testing = false, wasTesting = true;
    while (true) {
        ++cycleCount;
        _viGem360Pad->updateState();
        
        if (wasTesting != testing) {
            wasTesting = testing;
            clearScreen();
            cycleCount = 0;
            rumbleCommands = 0;
            if (testing)
                printXy(0, LINE_PHYSICAL_CTRL + 3, RED, "TESTING VIBRATIONS ONLY");
            else
                printXy(1, LINE_EVENT_STATUS, GREEN, "Processing events.\n");
        }

        if (testing) {
            UCHAR left, right;
			_viGem360Pad->getAnalogueAsByte(&left, &right);
            _viGem360Pad->setRumble(left, right);
            _viGem360Pad->getRumbleState(&rumbleCommands, &rumbleLeft, &rumbleRight, &rumblePlug);

            printBar(YELLOW, LINE_PHYSICAL_CTRL + 4, "Large motor:", (100 * left) / 255);
			printBar(YELLOW, LINE_PHYSICAL_CTRL + 5, "Small motor:", (100 * right) / 255);
            printBar(RED,    LINE_PHYSICAL_CTRL + 6, "Buttplug:   ", rumblePlug);
            printXy(0, LINE_PHYSICAL_CTRL + 7, RED, "Use left and right analog sticks");
            
            if (_kbhit() && _getch())
                testing = false;
        } else if (_viGem360Pad->getRumbleState(&rumbleCommands, &rumbleLeft, &rumbleRight, &rumblePlug) || (cycleCount == 0)) {
            printXy(1, LINE_RUMBLE_STATUS, WHITE, "Rumble instructions: %d\nStatus: L=%3d, R=%3d => Plug=%3d", rumbleCommands, rumbleLeft, rumbleRight, rumblePlug);
        }

        if ((cycleCount % 5) == 0) {
            if (_kbhit() || (cycleCount == 0)) {
				int key = _kbhit() ? _getch() : 0;
                if ((key == 'q') || (key == 'a') || (key == '+') || (key == '-')) {
                    int left = 0, right = 0;
                    if (key == 'q')
                        left = RUMBLE_STEP;
                    else if (key == 'a')
                        left = -RUMBLE_STEP;
                    else if (key == '+')
                        right = RUMBLE_STEP;
                    else if (key == '-')
                        right = -RUMBLE_STEP;
                    _viGem360Pad->adjustRumble(left, right);
                } else if (key == 't')
                    testing = true;
                int rumbleScaleLeft, rumbleScaleRight;
                _buttplugConfig->getVibration(&rumbleScaleLeft, &rumbleScaleRight);
                printXy(cols - 20, LINE_KEY_HELP + 0, WHITE, "Vib L/R: \x1B[%02Xm%3d%% / %3d%%", YELLOW, rumbleScaleLeft, rumbleScaleRight);
                printXy(cols - 20, LINE_KEY_HELP + 1, WHITE, "Q/A: Left, +/-: Right");
                printXy(cols - 20, LINE_KEY_HELP + 2, WHITE, "T: Test Vibrations");

                printXy(1, LINE_VIRTUAL_CTRL, GREEN, "Virtual controller index %d.", virtualPadIndex + 1);
                printXy(1, LINE_PHYSICAL_CTRL, WHITE, "Attached to \x1B[%02Xmphysical controller %d!", GREEN, usePhysicalPad + 1);
            }
        }

        if ((cycleCount % 200) == 0) {
            if (_buttplugDevice->checkConnectionStatus() == BP_CONNECTED) {
                printXy(1, 15, GREEN, "Buttplug connected.                   ");
                if ((cycleCount % 2000) == 0)
                    _buttplugDevice->readBatteryLevel();
            } else
                printXy(1, 15, RED, "Buttplug disconnected, reconnecting...");
        }

        if ((lastBatteryLevel != _currentBatteryLevel) || (cycleCount == 0)) {
            lastBatteryLevel = _currentBatteryLevel;
            printBatteryLevel();
        }

        System::Sleep(10);
    }
    delete _viGem360Pad;
    _viGem360Pad = NULL;
}

void SteamPlugMain::onBatteryLevelReceived(int batteryLevel) {
	_currentBatteryLevel = batteryLevel;
}

void SteamPlugMain::printBar(Color col, int y, const char *prefix, int value) {
    int terminalWidth, rows;
    getTerminalSize(&terminalWidth, &rows);

    int barWidth = (int)(terminalWidth - 7 - strlen(prefix));
    if (barWidth >= 0) {
        int barFill = (value * barWidth) / 100;
        char barBuffer[256];
        memset(barBuffer, ' ', barWidth + 1);
        memset(barBuffer, '=', barFill);
        barBuffer[barFill] = ']';
        barBuffer[barWidth + 1] = '\0';
        printXy(1, y, col, "%s [%s%3d%%", prefix, barBuffer, value);
    }
}

void SteamPlugMain::printBatteryLevel() {
    Color col = GREEN;
    if (_currentBatteryLevel < 20)
        col = YELLOW;
    else if (_currentBatteryLevel < 10)
        col = RED;

    _currentBatteryLevel = std::clamp(_currentBatteryLevel, 0, 100);
	printBar(col, LINE_BAT_STATUS, "BAT:", _currentBatteryLevel);
}

void SteamPlugMain::atProgramExit() {
	if (_viGem360Pad != NULL) {
		delete _viGem360Pad;
		_viGem360Pad = NULL;
	}
    if (_buttplugDevice) {
		_buttplugDevice->setVibrate(0);
        System::Sleep(500);
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
