#include "BluetoothBase.h"
#include "System.h"

#pragma warning(disable: 4244)

BluetoothBase::BluetoothBase() {
	_wclBluetoothManager.SetMessageProcessing(mpAsync);
}

BluetoothBase::~BluetoothBase() {
}

CwclBluetoothRadio* BluetoothBase::getRadio() {
	CwclBluetoothRadio* Radio;
	const int Res = _wclBluetoothManager.GetLeRadio(Radio);
	if (Res != WCL_E_SUCCESS)
		error("Get working radio failed: 0x%X", Res);
	return Radio;
}

std::string BluetoothBase::getDeviceName(BtAddress address) {
	tstring DevNameWide;
	if (getRadio()->GetRemoteName(address, DevNameWide) == WCL_E_SUCCESS)
		return std::string(DevNameWide.begin(), DevNameWide.end());
	else
		return "Unknown Device";
}

std::string BluetoothBase::Mac2String(BtAddress Address) {
	char macAddress[32];
	sprintf(macAddress, "%02X:%02X:%02X:%02X:%02X:%02X", ((unsigned char*)&Address)[5], ((unsigned char*)&Address)[4], ((unsigned char*)&Address)[3], ((unsigned char*)&Address)[2], ((unsigned char*)&Address)[1], ((unsigned char*)&Address)[0]);
	return std::string(macAddress);
}

std::string BluetoothBase::UuidToString(wclGattUuid uuid) {
	char line[128];
	if (uuid.IsShortUuid)
		sprintf(line, "%04X", uuid.ShortUuid);
	else
		sprintf(line, "%08X-%04X-%04X-%04X-%02X%02X-%02X-%02X-%02X-%02X-%02X", uuid.LongUuid.Data1, uuid.LongUuid.Data2, uuid.LongUuid.Data3, uuid.LongUuid.Data4[0], uuid.LongUuid.Data4[1], uuid.LongUuid.Data4[2], uuid.LongUuid.Data4[3], uuid.LongUuid.Data4[4], uuid.LongUuid.Data4[5], uuid.LongUuid.Data4[6], uuid.LongUuid.Data4[7]);
	return line;
}

