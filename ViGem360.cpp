#include "ButtplugDiscovery.h"
#include "ButtplugDevice.h"
#include "ButtplugConfig.h"
#include "System.h"

#include <iostream>
#include <algorithm>
//
// Windows basic types 'n' fun
//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//
// Optional depending on your use case
//
#include <Xinput.h>

//
// The ViGEm API
//
#include <ViGEm/Client.h>
#include <conio.h>
#include "resource.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ViGEmClient.lib")
#pragma comment(lib,"XInput.lib")
#pragma comment(lib,"Xinput9_1_0.lib")

#define BUTTPLUG_VIBRATE_MAX 20

class ViGem360Pad {
public:
    ViGem360Pad(ButtplugDevice *buttplugDevice, int rumbleLeft, int rumbleRight);
    ~ViGem360Pad();

    int getVirtualPadUserIndex();

    void attachToPhysicalPad(DWORD physicalPadId);

    void updateState(const XINPUT_GAMEPAD& state);

    int _rumbleScaleLeft, _rumbleScaleRight;
private:
    void rumbleCallback(UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber);

    static VOID CALLBACK rumbleCallbackFn(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData);

    PVIGEM_CLIENT _client;
    PVIGEM_TARGET _outputPad;

    unsigned long _virtualPadPlayerIndex;
    int _rumbleInstructionCount;

    DWORD _physicalPadId;
	ButtplugDevice* _buttplugDevice;
};

ViGem360Pad::ViGem360Pad(ButtplugDevice* buttplugDevice, int rumbleLeft, int rumbleRight) 
    : _client(vigem_alloc()), _outputPad(NULL), _physicalPadId(0xFFFF), _buttplugDevice(buttplugDevice), _rumbleInstructionCount(0), _rumbleScaleLeft(rumbleLeft), _rumbleScaleRight(rumbleRight) {

    _client = vigem_alloc();
    if (_client == nullptr)
        error("vigem_alloc failed!\n");
     
    VIGEM_ERROR retval = vigem_connect(_client);
    if (!VIGEM_SUCCESS(retval))
		error("ViGEm Bus connection failed with error code: 0x%X\n", retval);
    
    log("Setting up virtual x360 controller...");
    //
    // Allocate handle to identify new pad
    //
    _outputPad = vigem_target_x360_alloc();
	if (!_outputPad)
		error("vigem_target_x360_alloc failed!\n");

    retval = vigem_target_add(_client, _outputPad);
	if (!VIGEM_SUCCESS(retval))
		error("Virtual controller plug-in failed: 0x%X", retval);
    log("okay!\n");

	retval = vigem_target_x360_get_user_index(_client, _outputPad, &_virtualPadPlayerIndex);
	if (!VIGEM_SUCCESS(retval))
		error("Unable to get virtual controller index: 0x%X", retval);

    retval = vigem_target_x360_register_notification(_client, _outputPad, &rumbleCallbackFn, this);
    if (!VIGEM_SUCCESS(retval))
        error("Registering for notification failed with error code: 0x%X", retval);
}

ViGem360Pad::~ViGem360Pad() {
	vigem_target_remove(_client, _outputPad);
	vigem_target_free(_outputPad);

    vigem_disconnect(_client);
    vigem_free(_client);
}

int ViGem360Pad::getVirtualPadUserIndex() {
    return (int)_virtualPadPlayerIndex;
}

void ViGem360Pad::attachToPhysicalPad(DWORD physicalPadId) {
    _physicalPadId = physicalPadId;
}

void ViGem360Pad::updateState(const XINPUT_GAMEPAD& state) {
	// The XINPUT_GAMEPAD structure is identical to the XUSB_REPORT structure
	// so we can simply take it "as-is" and cast it.
	//
	// Call this function on every input state change e.g. in a loop polling
	// another joystick or network device or thermometer or... you get the idea.
	vigem_target_x360_update(_client, _outputPad, *reinterpret_cast<const XUSB_REPORT*>(&state));
}

void ViGem360Pad::rumbleCallback(UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber) {
    int cursorX, cursorY;
    getTerminalCursorPosition(&cursorX, &cursorY);

	int vibrate = (_rumbleScaleLeft * LargeMotor) / 255 + (_rumbleScaleRight * SmallMotor) / 255;
    vibrate = (vibrate * BUTTPLUG_VIBRATE_MAX) / 100;
    if (vibrate > BUTTPLUG_VIBRATE_MAX)
        vibrate = BUTTPLUG_VIBRATE_MAX;
    log("\033[%d;%dHVib: L=%3d, R=%3d => Plug=%3d\033[%d;%dH\n", 13, 1, LargeMotor, SmallMotor, vibrate, cursorY + 1, cursorX + 1);
    _buttplugDevice->setVibrate(vibrate);

    if (_physicalPadId != 0xFFFF) {
        XINPUT_VIBRATION vibration;
        vibration.wLeftMotorSpeed = (static_cast<WORD>(LargeMotor) << 8) | LargeMotor;
        vibration.wRightMotorSpeed = (static_cast<WORD>(SmallMotor) << 8) | SmallMotor;
        XInputSetState(_physicalPadId, &vibration);
    }
    
    log("\033[%d;%dHRumble instructions: %d\033[%d;%dH", 12, 1, ++_rumbleInstructionCount, cursorY + 1, cursorX + 1);
}

VOID CALLBACK ViGem360Pad::rumbleCallbackFn(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData) {
	((ViGem360Pad*)UserData)->rumbleCallback(LargeMotor, SmallMotor, LedNumber);
}

static ViGem360Pad* g_ViGem360Pad = NULL;

void __cdecl exitProg() {
    delete g_ViGem360Pad;
}

ButtplugConfig* getButtplugDevice() {
	ButtplugConfig* config = ButtplugConfig::fromFile();
    if (config == NULL) {
        log("No Buttplug address found in config, running Discovery!\n");
        
        ButtplugDiscovery discovery;
		if (!discovery.runDiscovery())
			error("No Buttplug device found!\n");

        config = discovery.getAsConfiguration();
        config->toFile();
        Sleep(3000);
        clearScreen();
        printf("\033[%d;%dH", 2, 1);
    }
    return config;
}

void batteryCallback(void* arg, int batteryLevel) {
    int cursorX, cursorY;
	getTerminalCursorPosition(&cursorX, &cursorY);

    int cols, rows;
    getTerminalSize(&cols, &rows);
    Color col = (batteryLevel < 15) ? RED : GREEN;

    char buffer[256];
	char* pos = buffer;
    pos += sprintf(pos, "\033[%d;%dH", 1, 1); // y 1, x 1
	pos += sprintf(pos, "BAT: ");
    pos += sprintf(pos, "\x1B[%02Xm", col);
	int barWidth = cols - 12;
	int barFill = (batteryLevel * barWidth) / 100;
    pos += sprintf(pos, "[");
	for (int i = 0; i < barFill; i++)
		pos += sprintf(pos, "=");
    pos += sprintf(pos, "]");
	for (int i = barFill; i < barWidth; i++)
		pos += sprintf(pos, " ");
    pos += sprintf(pos, "%3d%% ", batteryLevel);
    pos += sprintf(pos, "\033[%d;%dH", 11, 1);
    pos += sprintf(pos, "\x1B[%02Xm", WHITE);
    pos += sprintf(pos, "\033[%d;%dH", cursorY + 1, cursorX + 1);
    printf("%s", buffer);
}

int getPhysicalControllerIndex(int virtualPadId = -1) {
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
		if (i == virtualPadId)
			continue;

        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS)
            return i;
    }
	return -1;
}

#define RUMBLE_STEP 2

int main() {
    SetConsoleTitle(L"Buttplug for Steam X360 emulation");
	enableVirtualTerminalMode();
    setTerminalCursorVisibility(false);

    XINPUT_STATE state;

    log("\nLooking for X360 controllers...");
    int usePhysicalPad = getPhysicalControllerIndex();
    if (usePhysicalPad >= 0) {
        log("Found, attaching to \x1B[%02Xmphysical controller %d\x1B[%02Xm.\n", GREEN, usePhysicalPad + 1, WHITE);
    } else {
        log("\n\x1B[%02XmNo X360 gamepads detected!\x1B[%02Xm\n", YELLOW, WHITE);
    }

    ButtplugConfig* buttplugConfig = getButtplugDevice();
    ButtplugDevice buttplugDevice(buttplugConfig->getMacAddress(), buttplugConfig->getButtplugDefinition());
    buttplugDevice.setBatteryCallback(batteryCallback, NULL);
    buttplugDevice.waitForConnection();

    buttplugDevice.setVibrate(10);
    Sleep(300);
    buttplugDevice.setVibrate(5);
    Sleep(300);
    buttplugDevice.setVibrate(0);
    log("okay!\n");
 
    buttplugDevice.readBatteryLevel();

    atexit(exitProg);
    int rumbleLeft, rumbleRight;
    buttplugConfig->getVibration(&rumbleLeft, &rumbleRight);
	g_ViGem360Pad = new ViGem360Pad(&buttplugDevice, rumbleLeft, rumbleRight);
    int virtualPadIndex = g_ViGem360Pad->getVirtualPadUserIndex();
	log("Installed X360 controller. \x1B[%02XmVirtual controller index %d\x1B[%02Xm.\n", GREEN, virtualPadIndex + 1, WHITE);
    if (usePhysicalPad >= 0)
		g_ViGem360Pad->attachToPhysicalPad(usePhysicalPad);
    else {
        log("\x1B[%02XmWaiting for physical pad...\x1B[%02Xm", YELLOW, WHITE);
		while ((usePhysicalPad = getPhysicalControllerIndex(virtualPadIndex)) < 0)
			Sleep(1000);
        log("attached to \x1B[%02Xmphysical controller %d!\x1B[%02Xm", GREEN, usePhysicalPad + 1, WHITE);
        g_ViGem360Pad->attachToPhysicalPad(usePhysicalPad);
    }

    int cols, rows;
	getTerminalSize(&cols, &rows);
    
    int cycleCount = -1;
    log("\033[%d;%dH\x1B[%02XmButtplug connected.\x1B[%02Xm\n", 15, 1, GREEN, WHITE);
    log("\033[%d;%dH\x1B[%02XmProcessing events.\x1B[%02Xm\n", 16, 1, GREEN, WHITE);
    DWORD dwLastPacketId = 0;
    while (true) {
        // Grab the input from a physical X360 pad in this example
        if (XInputGetState(usePhysicalPad, &state) != ERROR_SUCCESS)
            error("Error reading from X360 pad %d!", usePhysicalPad + 1);
            
        if (dwLastPacketId != state.dwPacketNumber) {
            dwLastPacketId = state.dwPacketNumber;
			g_ViGem360Pad->updateState(state.Gamepad);
        }
        
        ++cycleCount;
        if ((cycleCount % 5) == 0) {
            if (_kbhit() || (cycleCount == 0)) {
                int key = _kbhit() ? _getch() : 0;
                if ((key == 'q') || (key == 'a') || (key == '+') || (key == '-')) {
                    if (key == 'q')
                        g_ViGem360Pad->_rumbleScaleLeft = std::clamp(g_ViGem360Pad->_rumbleScaleLeft + RUMBLE_STEP, 0, 200);
                    else if (key == 'a')
                        g_ViGem360Pad->_rumbleScaleLeft = std::clamp(g_ViGem360Pad->_rumbleScaleLeft - RUMBLE_STEP, 0, 200);
                    else if (key == '+')
                        g_ViGem360Pad->_rumbleScaleRight = std::clamp(g_ViGem360Pad->_rumbleScaleRight + RUMBLE_STEP, 0, 200);
                    else if (key == '-')
                        g_ViGem360Pad->_rumbleScaleRight = std::clamp(g_ViGem360Pad->_rumbleScaleRight - RUMBLE_STEP, 0, 200);
					buttplugConfig->setVibration(g_ViGem360Pad->_rumbleScaleLeft, g_ViGem360Pad->_rumbleScaleRight);
					buttplugConfig->toFile();
                }
                printXy(cols - 20, 13, WHITE, "Vib L/R: ");
                printXy(cols - 10, 13, YELLOW, "%3d%% / %3d%%", g_ViGem360Pad->_rumbleScaleLeft, g_ViGem360Pad->_rumbleScaleRight);
                printXy(cols - 20, 14, WHITE, "Left: Q/A, Right: +/-");
            }
        }
        if ((cycleCount % 200) == 0) {
			if (buttplugDevice.checkConnectionStatus() != BP_CONNECTED) {
				log("\033[%d;%dH\x1B[%02XmButtplug disconnected, reconnecting...\x1B[%02Xm\n", 15, 1, RED, WHITE);
            } else {
                log("\033[%d;%dH\x1B[%02XmButtplug connected.\x1B[%02Xm                   \n", 15, 1, GREEN, WHITE);
                if ((cycleCount % 2000) == 0)
                    buttplugDevice.readBatteryLevel();
            }
        }

        Sleep(10);
    }
    delete g_ViGem360Pad;
    g_ViGem360Pad = NULL;
}
