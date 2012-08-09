#include <map>
#include "PoomContactsReader.h"
#include "PoomContact.h"

#define BUF_SIZE 11
#define MAX_DATE_DOUBLE_4000 767011.083333

CPoomContactsReader::CPoomContactsReader(IFolder* pIFolder)
	:	IPoomFolderReader(pIFolder)
{
	_items = IPoomFolderReader::getItemCollection();
}

CPoomContactsReader::~CPoomContactsReader(void)
{
	if (_items)
		_items->Release();
}

int CPoomContactsReader::Count()
{
	int uCount = 0;
	_items->get_Count(&uCount);
	return uCount;
}

HRESULT CPoomContactsReader::Get(int i, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *puBufLen)
{
	HRESULT hr = E_FAIL;
	if (_items == NULL || pBuf == NULL || puBufLen == NULL) {
		DBG_TRACE(L"Debug - PoomContactsReader.cpp - Get(...) err 1 \n", 5, FALSE);
		return hr;
	}

	IContact* pElementContact = NULL;

	hr = _items->Item(i, (IDispatch **) &pElementContact);
	if (hr != S_OK) {
		DBG_TRACE(L"Debug - PoomContactsReader.cpp - Get(...) err 2 \n", 5, FALSE);
		return hr;
	}

	CPoomContact *contact = new(std::nothrow) CPoomContact();
	if (contact == NULL) {
		DBG_TRACE(L"Debug - PoomContactsReader.cpp - Get(...) err 3 \n", 5, FALSE);
		pElementContact->Release();
		return hr;
	}

	HeaderStruct *header = contact->Header();
	
	hr = pElementContact->get_Oid(&header->lOid);
	if (hr == S_OK) {
		Parse(pElementContact, contact);
		*pBuf = pPoomSerializer->Serialize(contact, (LPDWORD) puBufLen);
	} else {
		DBG_TRACE(L"Debug - PoomContactsReader.cpp - Get(...) err 4 \n", 5, FALSE);
	}

	if (contact)	
		delete contact;
	
	if (pElementContact)
		pElementContact->Release();

	return hr;
}

HRESULT CPoomContactsReader::GetOne(IPOutlookApp *pIPoomApp, LONG lOid, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *puBufLen)
{
	HRESULT hr = E_FAIL;
	if (pIPoomApp == NULL || pBuf == NULL || puBufLen == NULL)
		return hr;
	
	IContact *pElementContact = NULL;
	IItem *pItem = NULL;

	hr = pIPoomApp->GetItemFromOid(lOid, (IDispatch **) &pElementContact);
	if (hr != S_OK) {
		DBG_TRACE(L"Debug - PoomContactsReader.cpp - GetOne(...) err 1 \n", 5, FALSE);
		return hr;
	}

	if (SUCCEEDED(pElementContact->QueryInterface(IID_IItem, (void**)&pItem))) {
		INT iType = 0;
		BOOL bSIM = FALSE;
		
		if (SUCCEEDED(pItem->get_DefaultItemType(&iType))) {
			if (iType == 103)	// se abbiamo il tipo olSimContactItem (103) lo ignoriamo 
				bSIM = TRUE;
		}

		pItem->Release();
		pItem = NULL;

		if (bSIM == TRUE) {
			pElementContact->Release();
			return E_FAIL;
		}
	}

	CPoomContact* contact = new(std::nothrow) CPoomContact();
	if (contact == NULL) {
		DBG_TRACE(L"Debug - PoomContactsReader.cpp - GetOne(...) err 2 \n", 5, FALSE);
		pElementContact->Release();
		return E_FAIL;
	}
	
	HeaderStruct* header = contact->Header();
	header->lOid = lOid;
	Parse(pElementContact, contact);
	*pBuf = pPoomSerializer->Serialize(contact, (LPDWORD) puBufLen);

	if (contact) 
		delete contact;
	
	if (pElementContact)
		pElementContact->Release();

	return S_OK;
}

void CPoomContactsReader::Parse(IContact *iContact, CPoomContact *contact)
{
	DATE date;
	SYSTEMTIME st;
	ZeroMemory(&date, sizeof(DATE));
	ZeroMemory(&st, sizeof(SYSTEMTIME));

	ContactMapType* contactMap = contact->Map();

	BSTR bstrTemp;

#define GET_ENTRY(func, e_type, pMap)				\
	do {											\
		if(SUCCEEDED(func(&bstrTemp))){				\
			if(SysStringLen(bstrTemp) > 0){			\
				LPWSTR lpwString = new WCHAR[SysStringLen(bstrTemp) + 1];				\
				StringCchCopy(lpwString, SysStringLen(bstrTemp) + 1,(LPWSTR) bstrTemp);	\
				(*pMap)[e_type] = lpwString;			\
			}											\
		}												\
		SysFreeString(bstrTemp);						\
	} while (0)
	
	GET_ENTRY(iContact->get_FirstName, FirstName, contactMap);
	GET_ENTRY(iContact->get_LastName, LastName, contactMap);
	GET_ENTRY(iContact->get_CompanyName, CompanyName, contactMap);
	GET_ENTRY(iContact->get_BusinessFaxNumber, BusinessFaxNumber, contactMap);

	GET_ENTRY(iContact->get_Department, Department, contactMap);
	GET_ENTRY(iContact->get_Email1Address, Email1Address, contactMap);
	GET_ENTRY(iContact->get_MobileTelephoneNumber, MobileTelephoneNumber, contactMap);
	GET_ENTRY(iContact->get_OfficeLocation, OfficeLocation, contactMap);

	GET_ENTRY(iContact->get_PagerNumber, PagerNumber, contactMap);
	GET_ENTRY(iContact->get_BusinessTelephoneNumber, BusinessTelephoneNumber, contactMap);
	GET_ENTRY(iContact->get_JobTitle, JobTitle, contactMap);
	GET_ENTRY(iContact->get_HomeTelephoneNumber, HomeTelephoneNumber, contactMap);

	GET_ENTRY(iContact->get_Email2Address, Email2Address, contactMap);
	GET_ENTRY(iContact->get_Spouse, Spouse, contactMap);
	GET_ENTRY(iContact->get_Email3Address, Email3Address, contactMap);
	GET_ENTRY(iContact->get_Home2TelephoneNumber, Home2TelephoneNumber, contactMap);

	GET_ENTRY(iContact->get_HomeFaxNumber, HomeFaxNumber, contactMap);
	GET_ENTRY(iContact->get_CarTelephoneNumber, CarTelephoneNumber, contactMap);
	GET_ENTRY(iContact->get_AssistantName, AssistantName, contactMap);
	GET_ENTRY(iContact->get_AssistantTelephoneNumber, AssistantTelephoneNumber, contactMap);

	GET_ENTRY(iContact->get_Children, Children, contactMap);
	GET_ENTRY(iContact->get_Categories, Categories, contactMap);
	GET_ENTRY(iContact->get_WebPage, WebPage, contactMap);
	GET_ENTRY(iContact->get_Business2TelephoneNumber, Business2TelephoneNumber, contactMap);

	GET_ENTRY(iContact->get_RadioTelephoneNumber, RadioTelephoneNumber, contactMap);
	GET_ENTRY(iContact->get_FileAs, FileAs, contactMap);
	GET_ENTRY(iContact->get_YomiCompanyName, YomiCompanyName, contactMap);
	GET_ENTRY(iContact->get_YomiFirstName, YomiFirstName, contactMap);

	GET_ENTRY(iContact->get_YomiLastName, YomiLastName, contactMap);
	GET_ENTRY(iContact->get_Title, Title, contactMap);
	GET_ENTRY(iContact->get_MiddleName, MiddleName, contactMap);
	GET_ENTRY(iContact->get_Suffix, Suffix, contactMap);

	GET_ENTRY(iContact->get_HomeAddressStreet, HomeAddressStreet, contactMap);
	GET_ENTRY(iContact->get_HomeAddressCity, HomeAddressCity, contactMap);
	GET_ENTRY(iContact->get_HomeAddressState, HomeAddressState, contactMap);
	GET_ENTRY(iContact->get_HomeAddressPostalCode, HomeAddressPostalCode, contactMap);

	GET_ENTRY(iContact->get_HomeAddressCountry, HomeAddressCountry, contactMap);
	GET_ENTRY(iContact->get_OtherAddressStreet, OtherAddressStreet, contactMap);
	GET_ENTRY(iContact->get_OtherAddressCity, OtherAddressCity, contactMap);
	GET_ENTRY(iContact->get_OtherAddressPostalCode, OtherAddressPostalCode, contactMap);

	GET_ENTRY(iContact->get_OtherAddressCountry, OtherAddressCountry, contactMap);
	GET_ENTRY(iContact->get_BusinessAddressStreet, BusinessAddressStreet, contactMap);
	GET_ENTRY(iContact->get_BusinessAddressCity, BusinessAddressCity, contactMap);
	GET_ENTRY(iContact->get_BusinessAddressState, BusinessAddressState, contactMap);

	GET_ENTRY(iContact->get_BusinessAddressPostalCode, BusinessAddressPostalCode, contactMap);
	GET_ENTRY(iContact->get_BusinessAddressCountry, BusinessAddressCountry, contactMap);
	GET_ENTRY(iContact->get_CompanyName, Body, contactMap);
	GET_ENTRY(iContact->get_CompanyName, CompanyName, contactMap);

	GET_ENTRY(iContact->get_OtherAddressState, OtherAddressState, contactMap);
	GET_ENTRY(iContact->get_Body, Body, contactMap);

	if(SUCCEEDED(iContact->get_Birthday(&date))) {
		if(date != DATE_NONE && date < MAX_DATE_DOUBLE_4000){
			WCHAR *tmp1 = new WCHAR[BUF_SIZE];
			VariantTimeToSystemTime(date, &st);
			_snwprintf(tmp1, BUF_SIZE, L"%d/%d/%d", st.wDay, st.wMonth, st.wYear);
			(*contactMap)[Birthday] = tmp1;
		}
	}

	if(SUCCEEDED(iContact->get_Anniversary(&date))) {
		if(date != DATE_NONE && date < MAX_DATE_DOUBLE_4000){
			WCHAR *tmp2 = new WCHAR[BUF_SIZE];
			VariantTimeToSystemTime(date, &st);
			_snwprintf(tmp2, BUF_SIZE, L"%d/%d/%d", st.wDay, st.wMonth, st.wYear);
			(*contactMap)[Anniversary] = tmp2; 
		}
	}
}