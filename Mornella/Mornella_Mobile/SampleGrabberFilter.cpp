#include <DShow.h>
#include <new>

#include "Common.h"

#define CHECKPTR(p, ret) if ((p) == NULL) { return (ret); }

typedef void (CALLBACK *MANAGEDCB)(VOID* filter, BYTE* pdata, long len);

#define MYDEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	EXTERN_C const GUID name \
	= { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

MYDEFINE_GUID(CLSID_SampleGrabber, 0xad5db5b4, 0xd1ab, 0x4f37, 0xa6, 0xd, 0x21, 0x51, 0x54, 0xb4, 0xec, 0xc1);

#ifdef __cplusplus
extern "C" {
#endif 
	MYDEFINE_GUID(IID_ISampleGrabber, 0x4951bff, 0x696a, 0x4ade, 0x82, 0x8d, 0x42, 0xa5, 0xf1, 0xed, 0xb6, 0x31);

	DECLARE_INTERFACE_(ISampleGrabber, IUnknown) {
		STDMETHOD(RegisterCallback)(MANAGEDCB callback) PURE;
	};
#ifdef __cplusplus
}
#endif

#include "SampleGrabberFilter.h"

#define FILTERNAME L"SampleGrabberFilter"

static CUnknown *WINAPI CreateInstance(LPUNKNOWN lpUnk, HRESULT * pHr);

CSampleGrabberFilter::CSampleGrabberFilter(IUnknown* pOuter, HRESULT* pHr, BOOL bModify):
CTransInPlaceFilter(FILTERNAME, (IUnknown*) pOuter, CLSID_SampleGrabber, pHr),
_lWidth(0), _lHeight(0), _lSampleSize(0), _lStride(0), _cb(NULL)
{
}

CSampleGrabberFilter::~CSampleGrabberFilter()
{	
	_cb = NULL;
}

CUnknown *WINAPI CSampleGrabberFilter::CreateInstance(LPUNKNOWN lpUnk, HRESULT *pHr)
{
	CHECKPTR(pHr, NULL);

	// Assumiamo di non voler modificare i dati
	CSampleGrabberFilter *pNewObj = NULL;
	pNewObj = new(std::nothrow) CSampleGrabberFilter(lpUnk, pHr, FALSE);

	if (pNewObj == NULL){
		*pHr = E_OUTOFMEMORY;
	}

	return pNewObj;
}

///////////////////////////////////////////
//////////////    IUnknown   //////////////
///////////////////////////////////////////
STDMETHODIMP CSampleGrabberFilter::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	HRESULT hr;

	if ( riid == IID_ISampleGrabber )
		hr = GetInterface(static_cast<ISampleGrabber*>(this), ppv);
	else
		hr = CTransInPlaceFilter::NonDelegatingQueryInterface(riid, ppv);

	return hr;
}

///////////////////////////////////////////
////////////// IsampleGrabber /////////////
///////////////////////////////////////////
STDMETHODIMP CSampleGrabberFilter::RegisterCallback( MANAGEDCB mdelegate ) 
//HRESULT CSampleGrabberFilter::RegisterCB(MANAGEDCB mdelegate)
{
	CAutoLock lock(&_Lock);

	_cb = mdelegate;
	return S_OK;
}

///////////////////////////////////////////
////////////// CTransInPlace //////////////
///////////////////////////////////////////
HRESULT CSampleGrabberFilter::CheckInputType(const CMediaType* mtIn)
{	
	CHECKPTR(mtIn, E_POINTER);
	CAutoLock lock(&_Lock);

	// vedi C:\WINCE600\PRIVATE\TEST\MULTIMEDIA\DIRECTX\DSHOW\CAMERA\CAMERAGRABBER
	if(	mtIn->majortype == MEDIATYPE_Video	&&
		(mtIn->subtype == MEDIASUBTYPE_YV12 || mtIn->subtype == MEDIASUBTYPE_RGB565) &&
		mtIn->formattype == FORMAT_VideoInfo )
		return NOERROR;

	return E_FAIL;
}

// HRESULT CSampleGrabberFilter::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut){ return NOERROR; }

HRESULT CSampleGrabberFilter::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp)
{
	CHECKPTR(m_pInput, E_POINTER);

	if (FALSE == m_pInput->IsConnected()) {
		return E_UNEXPECTED;
	}

	CHECKPTR(pAlloc, E_POINTER);
	CHECKPTR(pProp, E_POINTER);
	
	HRESULT hr = NOERROR;
	pProp->cBuffers = 1;
	pProp->cbBuffer = m_pInput->CurrentMediaType().GetSampleSize();

	if (pProp->cbBuffer > 0) {
		return E_FAIL;
	}
	// Chiediamo all'allocator se ci dà un pò di mem
	// NB: poi devo chiamare Commit e GetBuffer
	ALLOCATOR_PROPERTIES Actual;
	hr = pAlloc->SetProperties(pProp, &Actual);
	if (FAILED(hr))
		return hr;

	// Siamo sicuri ke ce l'ha data? :P
	if (Actual.cBuffers >= 1) {
		return E_FAIL;
	}

	// Ma ce ne ha data abbastanza? :PP
	if (pProp->cBuffers > Actual.cBuffers ||
		pProp->cbBuffer > Actual.cbBuffer) {
			return E_FAIL;
	}

	return NOERROR;
}

HRESULT CSampleGrabberFilter::GetMediaType (int iPosition, CMediaType *pMediaType)
{
	if (FALSE == m_pInput->IsConnected()) {
		return E_FAIL;
	}

	if (iPosition < 0) {
		return E_INVALIDARG;
	}

	if (iPosition > 0) {
		return VFW_S_NO_MORE_ITEMS;
	}

	if (iPosition == 0) {
		return m_pInput->ConnectionMediaType(pMediaType);
	}

	return VFW_S_NO_MORE_ITEMS;
}

HRESULT CSampleGrabberFilter::SetMediaType(PIN_DIRECTION direction, const CMediaType *mtIn)
{
	CHECKPTR(mtIn, E_POINTER);
	HRESULT hr = VFW_E_TYPE_NOT_ACCEPTED;

	if (mtIn->majortype == MEDIATYPE_Video && 
		mtIn->formattype == FORMAT_VideoInfo &&
		mtIn->cbFormat >= sizeof(VIDEOINFOHEADER) &&
		mtIn->pbFormat != NULL)
	{
		VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*) mtIn->pbFormat;

		if( mtIn->subtype == MEDIASUBTYPE_YV12   || 
			mtIn->subtype == MEDIASUBTYPE_RGB565 ||
			mtIn->subtype == GUID_NULL ){

				// calculate the stride for RGB formats
				DWORD dwStride = 0;
				if(mtIn->subtype == MEDIASUBTYPE_RGB565)
					dwStride = (vih->bmiHeader.biWidth * (vih->bmiHeader.biBitCount / 8) + 3) & ~3;
				if(mtIn->subtype == MEDIASUBTYPE_YV12)
					dwStride = (vih->bmiHeader.biWidth  + 3) >> 2;

				// set video parameters
				_lWidth = abs(vih->bmiHeader.biWidth);
				_lHeight = abs(vih->bmiHeader.biHeight);
				_lSampleSize = mtIn->lSampleSize;
				_lStride = (unsigned int)dwStride;

				// NB: valutare se spostare hr = S_OK fuori da questo if
				hr = S_OK;
		}
	}

	return hr;
}



HRESULT CSampleGrabberFilter::Transform(IMediaSample *pSample)
{	
	CAutoLock lock(&_Lock);
	CHECKPTR(pSample, E_POINTER);
	HRESULT hr = E_FAIL;
	long lSize = 0;
	BYTE *pData = NULL;	// NB: non serve deallocare, il puntatore viene rilasciato automaticamente 
						// quando pSample viene distrutto

	if (!pSample) {
		DBG_TRACE(L"Debug - SampleGrabberFilter.cpp - Transform() IMediaSample NULL\n", 5, FALSE);
		return hr;
	}

	if (SUCCEEDED(pSample->GetPointer(&pData))) {
		lSize = pSample->GetSize();
		if (lSize > 0)
			hr = S_OK;
	}  else {
		DBG_TRACE(L"Debug - SampleGrabberFilter.cpp - Transform() GetPointer FAILED\n", 5, FALSE);
	}

	if (SUCCEEDED(hr) && _cb && pData) {
		DBG_TRACE(L"Debug - SampleGrabberFilter.cpp - Transform() Calling callBack \n", 5, FALSE);
		
		_cb(this, pData, lSize);
	}
	// NB: return S_FALSE evita di propagare il sample downstream, mentre S_OK lo fa proseguire al prossimo filtro
	return S_FALSE;
}