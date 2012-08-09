class CSampleGrabberFilter: public CTransInPlaceFilter, public ISampleGrabber
{
private:
	MANAGEDCB _cb;

protected:
	CCritSec _Lock;

public:
	unsigned int _lWidth;
	unsigned int _lHeight;
	unsigned int _lSampleSize;
	unsigned int _lStride;

public:
	CSampleGrabberFilter(IUnknown* pOuter, HRESULT* pHr, BOOL bModify);
	~CSampleGrabberFilter();

	static CUnknown *WINAPI CreateInstance(LPUNKNOWN lpUnk, HRESULT* pHr);

	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
	STDMETHODIMP RegisterCallback(MANAGEDCB mdelegate);
	//HRESULT RegisterCB(MANAGEDCB mdelegate);

	LPAMOVIESETUP_FILTER GetSetupData(){ return NULL; };

	HRESULT CheckInputType(const CMediaType* mtIn);
	HRESULT CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut){ return NOERROR; }
	HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp);
	HRESULT GetMediaType (int iPosition, CMediaType *pMediaType);
	HRESULT SetMediaType(PIN_DIRECTION direction, const CMediaType *pmt);
	HRESULT Transform(IMediaSample *pSample);
};