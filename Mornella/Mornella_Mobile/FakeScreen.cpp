#include "FakeScreen.h"

#define INITGUID				 
#include <initguid.h>
#include <imgguids.h>
#undef INITGUID


HINSTANCE FakeScreen::g_hInst = 0;
RECT FakeScreen::coords = {0};
IImagingFactory* FakeScreen::m_pImageFactory = NULL;
IImage* FakeScreen::m_pImage = NULL;
HWND FakeScreen::hWnd = NULL;
ATOM FakeScreen::aWndClass = 0;

FakeScreen* FakeScreen::m_pInstance = NULL;
volatile LONG FakeScreen::lLock = 0;

FakeScreen* FakeScreen::self(HINSTANCE hAppInst)
{
	while (InterlockedExchange((LPLONG)&lLock, 1) != 0)
		Sleep(1);

	if (m_pInstance == NULL) {
		m_pInstance = new(std::nothrow) FakeScreen(hAppInst);
	}

	InterlockedExchange((LPLONG)&lLock, 0);

	return m_pInstance;
}

FakeScreen::FakeScreen(HINSTANCE hInst) : bInit(FALSE), hWndDesktop(0), hThread(NULL), lpBitmapBuf(NULL)
{
	g_hInst = hInst;
}

FakeScreen::~FakeScreen()
{
	Stop();
}

BOOL FakeScreen::InitBitmapHeader()
{
	HDC	hDCScreen;

	hDCScreen = GetDC(hWndDesktop);

	if(hDCScreen == NULL)
		return FALSE;

	rExtents.top		= 0;
	rExtents.left		= 0;
	rExtents.right		= GetDeviceCaps(hDCScreen, HORZRES);
	rExtents.bottom		= GetDeviceCaps(hDCScreen, VERTRES);

	x = (rExtents.right - rExtents.left) & ~3;
	y = rExtents.bottom - rExtents.top;

	// Riempiamo l'header della BMP
	pbmi = (LPBITMAPINFO) LocalAlloc(LPTR, sizeof(BITMAPINFO)+ (3 - 1) * sizeof(RGBQUAD));

	if(pbmi == NULL) {
		ReleaseDC(hWndDesktop, hDCScreen);
		return FALSE;
	}

	pbmi->bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth			= x;
	pbmi->bmiHeader.biHeight		= y;
	pbmi->bmiHeader.biPlanes		= 1;	
	pbmi->bmiHeader.biBitCount		= DEFAULT_BITSPIXEL;
	pbmi->bmiHeader.biCompression	= BI_RGB;

	// Maschera
	LPDWORD lpdwMaschera = (LPDWORD)(pbmi->bmiColors);
	lpdwMaschera[0] = 0x00007c00;
	lpdwMaschera[1] = 0x000003e0;
	lpdwMaschera[2] = 0x0000001f;
	pbmi->bmiHeader.biCompression = BI_BITFIELDS;

	// BitmapFileHeader
	DWORD dwImageDataSize = pbmi->bmiHeader.biWidth * pbmi->bmiHeader.biHeight * (pbmi->bmiHeader.biBitCount / 8);
	bfh.bfType		= 'M' << 8 | 'B';
	bfh.bfOffBits	= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO) + (3-1) * sizeof(RGBQUAD);
	bfh.bfSize		= bfh.bfOffBits + dwImageDataSize;

	ReleaseDC(hWndDesktop, hDCScreen);
	return TRUE;
}

// Prende lo screenshot e valorizza lpBuf che poi va liberato con una delete[]
BOOL FakeScreen::GetScreenshot(HBITMAP *hDIB, LPBYTE &lpBuf, UINT &uSize)
{
	HBITMAP	hBmpStock = NULL;
	HDC hDCMem = NULL;
	LPBYTE lpDibBuf = NULL;
	HDC	hDCScreen;

	uSize = 0;

	hDCScreen = GetDC(hWndDesktop);

	if(hDCScreen == NULL)
		return FALSE;

	// Crea un memory DC in cui verrà selezionata la DIB
	hDCMem = CreateCompatibleDC(hDCScreen);

	if(hDCMem == NULL) {
		ReleaseDC(hWndDesktop, hDCScreen);
		return FALSE;
	}

	// lpDibBuf va liberato con una DeleteObject(HGDIOBJ)
	*hDIB = CreateDIBSection(hDCMem, pbmi, DIB_RGB_COLORS, (LPVOID*)&lpDibBuf, NULL, 0);

	if(*hDIB == NULL || lpDibBuf == NULL) {
		DeleteDC(hDCMem);
		ReleaseDC(hWndDesktop, hDCScreen);
		return FALSE;
	}

	hBmpStock = (HBITMAP)SelectObject(hDCMem, *hDIB);

	if(hBmpStock == NULL) {
		ReleaseDC(hWndDesktop, hDCScreen);
		DeleteDC(hDCMem);
		return FALSE;
	}

	if(BitBlt(hDCMem, 0, 0, rExtents.right  - rExtents.left, rExtents.bottom - rExtents.top,
		hDCScreen, rExtents.left, rExtents.top, SRCCOPY) == 0) {
			ReleaseDC(hWndDesktop, hDCScreen);
			DeleteDC(hDCMem);
			DeleteObject(hBmpStock);
			return FALSE;
	}

	ReleaseDC(hWndDesktop, hDCScreen);
	DeleteDC(hDCMem);
	DeleteObject(hBmpStock);

	// Aggiungiamo l'header al buffer (lpBuf va poi liberato con una delete[])
	PBYTE pTmp;

	uSize = pbmi->bmiHeader.biWidth * pbmi->bmiHeader.biHeight * (pbmi->bmiHeader.biBitCount / 8);

	lpBuf = new(std::nothrow) BYTE[sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 3 + uSize];

	if(lpBuf == NULL) {
		uSize = 0;
		return FALSE;
	}

	pTmp = lpBuf;
	CopyMemory(pTmp, &bfh, sizeof(BITMAPFILEHEADER));
	pTmp += sizeof(BITMAPFILEHEADER);

	CopyMemory(pTmp, &pbmi->bmiHeader, sizeof(BITMAPINFOHEADER));
	pTmp += sizeof(BITMAPINFOHEADER);

	CopyMemory(pTmp, &pbmi->bmiColors, sizeof(RGBQUAD) * 3);
	pTmp += sizeof(RGBQUAD) * 3;

	CopyMemory(pTmp, lpDibBuf, uSize);
	uSize += sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 3;

	return TRUE;
}

DWORD WINAPI FakeScreen::MessagePump(LPVOID lpParam)
{
	MSG msg;
	FakeScreen *pFakeScreen = (FakeScreen *)lpParam;

	// Perform application initialization:
	if (!CreateScreenWindow(pFakeScreen->g_hInst)) {
		return FALSE;
	}

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int) msg.wParam;
}

BOOL FakeScreen::MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass)
{
	WNDCLASS wc;

	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = FakeScreen::WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = NULL;
	wc.hCursor       = 0;
	wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = szWindowClass;

	aWndClass = RegisterClass(&wc);

	if(aWndClass == 0 && GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
		UnregisterClass((LPCWSTR)aWndClass, 0);
		aWndClass = RegisterClass(&wc);
	}

	return (BOOL)aWndClass;
}

BOOL FakeScreen::CreateScreenWindow(HINSTANCE hInstance)
{
	g_hInst = hInstance; // Store instance handle in our global variable

	if (!MyRegisterClass(hInstance, L""))
		return FALSE;

	// Create a window
	hWnd = CreateWindowEx(WS_EX_TOPMOST, L"", NULL, WS_POPUP, 0, 0, 
		GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
		NULL, NULL, hInstance, NULL);

	if (!hWnd)
		return FALSE;

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);
	SetFocus(hWnd);

	return TRUE;
}

#include "winuserm.h"
LRESULT CALLBACK FakeScreen::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_CREATE:
			break;

		case WM_KEYDOWN:
			// Abilitare solo durante il debugging
			switch (wParam)
			{
			case VK_ACTION:
				PostMessage(hWnd, WM_CLOSE, 0, 0);
				return 0L;
			}
			break;

		case WM_PAINT:
			HDC hDC;
			PAINTSTRUCT paintStruct;

			hDC = BeginPaint(hWnd, &paintStruct);
			FakeScreen::m_pImage->Draw(hDC, &(FakeScreen::coords), NULL);
			EndPaint(hWnd, &paintStruct);
			break;

		case WM_USER + 0x666:
			PostQuitMessage(0);
			//return DefWindowProc(hWnd, WM_CLOSE, wParam, lParam);
			break;

		case WM_DESTROY:
			break;

		case WM_QUIT:
			break;

		case WM_ACTIVATE:
			break;

		case WM_SETTINGCHANGE:
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

BOOL FakeScreen::Start()
{
	ImageInfo info = {0};
	HBITMAP hDIB = NULL;
	UINT uSize;
	HRESULT hr = E_FAIL;

	if(bInit)
		return FALSE;

	bInit = TRUE;

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (hr == S_FALSE) {
		CoUninitialize();
		return FALSE;
	}

	if (FAILED(hr))
		return FALSE;

	if (!SUCCEEDED(CoCreateInstance(CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, 
		IID_IImagingFactory, (void **) &m_pImageFactory))) {
			return FALSE;
	}

	if(InitBitmapHeader() == FALSE)
		return FALSE;

	// Prendiamo lo screenshot
	if(GetScreenshot(&hDIB, lpBitmapBuf, uSize) == FALSE) {
		if(hDIB) {
			DeleteObject(hDIB);
			hDIB = 0;
		}

		return FALSE;
	}

	if(hDIB) {
		DeleteObject(hDIB);
		hDIB = 0;
	}

	if(!SUCCEEDED(m_pImageFactory->CreateImageFromBuffer(lpBitmapBuf, uSize, BufferDisposalFlagNone, &m_pImage)))
		return FALSE;

	if(!SUCCEEDED(m_pImage->GetImageInfo(&info)))
		return FALSE;

	coords.left = 0;
	coords.top = 0;
	coords.right = info.Width;
	coords.bottom = info.Height;

	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MessagePump, this, 0, NULL);

	if(hThread == NULL)
		return FALSE;

	Sleep(1000);

	return TRUE;
}

BOOL FakeScreen::Stop()
{
	if(bInit == FALSE)
		return FALSE;

	if(hThread != NULL) {
		SendMessage(hWnd, WM_USER + 0x666, NULL, NULL); 
		WaitForSingleObject(hThread, INFINITE);

		if(aWndClass)
			UnregisterClass((LPCWSTR)aWndClass, 0);

		CloseHandle(hThread);
	}

	if(pbmi) {
		LocalFree(pbmi);
		pbmi = NULL;
	}

	if(lpBitmapBuf) {
		delete[] lpBitmapBuf;
		lpBitmapBuf = NULL;
	}

	if(m_pImage) {
		m_pImage->Release(); 
		m_pImage = NULL;
	}

	if(m_pImageFactory) {
		m_pImageFactory->Release();
		m_pImageFactory = NULL;
	}

	CoUninitialize();
	bInit = FALSE;

	return TRUE;
}