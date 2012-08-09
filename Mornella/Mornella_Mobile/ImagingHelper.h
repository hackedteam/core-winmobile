#include <Windows.h>
#include <atlbase.h>

const GUID MCLSID_EncoderQuality = { 0x1d5be4b5,0xfa4a,0x452d, { 0x9c,0xdd,0x5d,0xb3,0x51,0x05,0xe7,0xeb } };

HRESULT GetEncoderFromName(	IImagingFactory* pImgFactory,
							LPWSTR lpStrEnc,
							GUID* guid )
{
	HRESULT hr = E_FAIL;
	ImageCodecInfo *encoders, *ePtr;
	unsigned int uEncCount = 0;
	ZeroMemory(guid,sizeof(GUID));

	hr = pImgFactory->GetInstalledEncoders(&uEncCount, &encoders);

	if(SUCCEEDED(hr) && uEncCount > 0){
		ePtr = encoders;

		for(unsigned int i = 0; i < uEncCount; i++){
			if( 0 == wcscmp(ePtr->FormatDescription, lpStrEnc)){
				CopyMemory(guid, &(ePtr->Clsid), sizeof(GUID));
				hr = S_OK;
				break;
			}
			ePtr++;
		}
	}

	ePtr = NULL;

	if(encoders)
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

	if(SUCCEEDED(hr)){
		hr = pImgFactory->CreateImageEncoderToStream(	(CLSID*)guidJpeg,
														*pOutStream,
														pImageEncoder );
	}

	return hr;
}

HRESULT SetEncoderQuality(CComPtr<IImageEncoder> pImageEncoder, long lQuality)
{
	//ASSERT(pImageEncoder);

	HRESULT hr = E_FAIL;

	if(pImageEncoder == NULL)
		return hr;

	EncoderParameters* ParamsList;
	unsigned int uParamNumber = 1;

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