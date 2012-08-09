#pragma once

#include <new>		   
using namespace std;
#include <Windows.h>
#include <imaging.h>

#define DEFAULT_BITSPIXEL	16
#define MAX_LOADSTRING 100
#define BYTESPERLINE(Width, BPP) ((WORD)((((DWORD)(Width) * (DWORD)(BPP) + 31) >> 5)) << 2)

class FakeScreen;

class FakeScreen {
private:
	static FakeScreen* m_pInstance;
	static volatile LONG lLock;

	IImage* g_pImage;
	LPBITMAPINFO		pbmi;
	BITMAPFILEHEADER bfh;
	RECT	rExtents;
	UINT	x, y;
	HWND hWndDesktop;
	HANDLE hThread;
	BOOL bInit;
	LPBYTE lpBitmapBuf;

	static ATOM aWndClass;
	static HINSTANCE g_hInst;
	static RECT coords;
	static IImagingFactory* m_pImageFactory;
	static IImage* m_pImage;
	static HWND hWnd;

private:
	BOOL TakeScreenshot();
	BOOL ShowScreenshot();
	static BOOL MyRegisterClass(HINSTANCE, LPTSTR);
	static BOOL CreateScreenWindow(HINSTANCE);
	BOOL InitBitmapHeader();
	BOOL GetScreenshot(HBITMAP *, LPBYTE &, UINT &);
	static LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
	static DWORD WINAPI MessagePump(LPVOID);

protected:
	FakeScreen(HINSTANCE);

public:
	BOOL Start();
	BOOL Stop();

	static FakeScreen* self(HINSTANCE); 
	~FakeScreen();
};