#pragma once
#include <windows.h>
#include <pm.h>
#include <regext.h>

// Event OnBackLight Slow
void BackLightCallbackSlow(POWER_BROADCAST *powerBroadcast, DWORD dwUserData);

// Event OnBackLight
void BackLightCallback(POWER_BROADCAST *powerBroadcast, DWORD dwUserData);

// Event OnBatteryLevel
void BatteryCallback(POWER_BROADCAST *powerBroadcast, DWORD dwUserData);

// Event OnAC
void AcCallback(POWER_BROADCAST *powerBroadcast, DWORD dwUserData);

// Agent CallLocal
void CallBackNotification(HREGNOTIFY hNotify, DWORD dwCallState, const PBYTE pData, const UINT cbData);