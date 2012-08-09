#include "PoomCalendarReader.h"
#include "PoomCalendar.h"

CPoomCalendarReader::CPoomCalendarReader(IFolder* pIFolder)
	:	IPoomFolderReader(pIFolder)
{
	_items = IPoomFolderReader::getItemCollection();
}

CPoomCalendarReader::~CPoomCalendarReader(void)
{
	if (_items)
		_items->Release();
}

int CPoomCalendarReader::Count()
{
	INT iCount = 0;
	_items->get_Count(&iCount);
	return iCount;
}

HRESULT CPoomCalendarReader::Get(int i, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *puBufLen)
{
	HRESULT  hr = E_FAIL;

	if (_items == NULL || pBuf == NULL || puBufLen == NULL) {
		DBG_TRACE(L"Debug - PoomCalendarReader.cpp - Get(...) err 1 \n", 5, FALSE);
		return hr;
	}

	IAppointment *pElementCalendar = NULL;

	if (FAILED(_items->Item(i, (IDispatch **) &pElementCalendar))) {
		DBG_TRACE(L"Debug - PoomCalendarReader.cpp - Get(...) err 2 \n", 5, FALSE);
		return hr;
	}

	CPoomCalendar *calendar = new(std::nothrow) CPoomCalendar();

	if (calendar == NULL) {
		DBG_TRACE(L"Debug - PoomCalendarReader.cpp - Get(...) err 3 \n", 5, FALSE);
		pElementCalendar->Release();
		return hr;
	}

	HeaderStruct *header = calendar->Header();

	if (SUCCEEDED(pElementCalendar->get_Oid(&header->lOid))){
		Parse(pElementCalendar, calendar);
		*pBuf = pPoomSerializer->Serialize(calendar, (LPDWORD) puBufLen);
	}  else {
		DBG_TRACE(L"Debug - PoomCalendarReader.cpp - Get(...) err 4 \n", 5, FALSE);
	}

	if (calendar) 
		delete calendar;

	pElementCalendar->Release();

	return S_OK;
}

HRESULT CPoomCalendarReader::GetOne(IPOutlookApp* pIPoomApp, LONG lOid, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT* puBufLen)
{
	HRESULT hr = E_FAIL;
	
	if (pIPoomApp == NULL || pBuf == NULL || puBufLen == NULL)
		return hr;

	IAppointment *pElementCalendar = NULL;

	hr = pIPoomApp->GetItemFromOid(lOid, (IDispatch **) &pElementCalendar);
	if (hr != S_OK) {
		DBG_TRACE(L"Debug - PoomCalendarReader.cpp - GetOne(...) err 1 \n", 5, FALSE);
		return hr;
	}

	CPoomCalendar *calendar = new(std::nothrow) CPoomCalendar();
	if (calendar == NULL) {
		DBG_TRACE(L"Debug - PoomCalendarReader.cpp - GetOne(...) err 1 \n", 5, FALSE);
		pElementCalendar->Release();
		return S_FALSE;
	}

	HeaderStruct *header = calendar->Header();
	header->lOid = lOid;
	Parse(pElementCalendar, calendar);
	*pBuf = pPoomSerializer->Serialize(calendar, (LPDWORD) puBufLen);

	if (calendar) 
		delete calendar;

	pElementCalendar->Release();

	return S_OK;
}

void CPoomCalendarReader::Parse(IAppointment *iAppointment, CPoomCalendar *calendar)
{
	DATE			dateTemp;
	LONG			longTemp;
	VARIANT_BOOL	variantBoolTemp;
	UINT			uintTemp;
	SYSTEMTIME		st;
	FILETIME		ft1;
	FILETIME		ft2;

	// VARIANT_BOOL
	if (SUCCEEDED(iAppointment->get_AllDayEvent(&variantBoolTemp))) {
		if (variantBoolTemp == VARIANT_TRUE)
			calendar->SetFlags( calendar->Flags() | FLAG_ALLDAY );
	}

	if (SUCCEEDED(iAppointment->get_ReminderSet(&variantBoolTemp))) {
		if (variantBoolTemp == VARIANT_TRUE)
			calendar->SetFlags( calendar->Flags() | FLAG_REMINDER );
	}

	// BSTR
	GETBSTR(iAppointment->get_Body, calendar->SetBody);
	GETBSTR(iAppointment->get_Categories, calendar->SetCategories);
	GETBSTR(iAppointment->get_Subject, calendar->SetSubject);
	GETBSTR(iAppointment->get_Location, calendar->SetLocation);

	// LONG
	if(SUCCEEDED(iAppointment->get_BusyStatus(&longTemp)))
		calendar->SetBusyStatus(&longTemp);

	if(SUCCEEDED(iAppointment->get_Duration(&longTemp)))
		calendar->SetDuration(&longTemp);

	if(SUCCEEDED(iAppointment->get_MeetingStatus(&longTemp)))
		calendar->SetMeetingStatus(&longTemp);

	if(SUCCEEDED(iAppointment->get_Sensitivity(&longTemp)))
		calendar->SetSensitivity(&longTemp);

	// DATE
	GETDATE(iAppointment->get_Start, calendar->SetStartDate);
	GETDATE(iAppointment->get_End, calendar->SetEndDate);

	// RECIPIENT
	if (SUCCEEDED(iAppointment->get_MeetingStatus(&longTemp))) {
		if (longTemp != olNonMeeting){
			calendar->SetFlags( calendar->Flags() | FLAG_MEETING );
			
			BSTR bstrTemp1 = NULL, bstrTemp2 = NULL;

			IRecipients *pIRecipients = NULL;
			
			if (SUCCEEDED(iAppointment->get_Recipients(&pIRecipients))) {
				INT count = 0;

				if (SUCCEEDED(pIRecipients->get_Count(&count))) {
					if (count > 0) {
						IRecipient *pIRecip = NULL;
						LPWSTR lpwRecipients = NULL, lpwStrTemp = NULL;
						INT len = 0, lenTmp = 0;
						
						// calculate total length
						for (INT i = 1; i <= count; i++) {					
							if (SUCCEEDED(pIRecipients->Item(i, &pIRecip))) {
								if (SUCCEEDED(pIRecip->get_Name(&bstrTemp1))) {
									len += SysStringLen(bstrTemp1);
									SysFreeString(bstrTemp1);
								}

								if (SUCCEEDED(pIRecip->get_Address(&bstrTemp2))) {
									len += SysStringLen(bstrTemp2);
									SysFreeString(bstrTemp2);
								}
							}

							if (pIRecip) {
								pIRecip->Release();
								pIRecip = NULL;
							}
						}

#define EXTRA_CHAR 7
						len += (EXTRA_CHAR*count);

						lpwRecipients = new WCHAR[len];
						ZeroMemory(lpwRecipients, len*sizeof(WCHAR));
						
						for (INT i = 0; i < count; i++) {
							if (SUCCEEDED(pIRecipients->Item(i+1, &pIRecip))) {
								lenTmp = 0;

								if (SUCCEEDED(pIRecip->get_Name(&bstrTemp1)))
									lenTmp += SysStringLen(bstrTemp1);

								if (SUCCEEDED(pIRecip->get_Address(&bstrTemp2)))
									lenTmp += SysStringLen(bstrTemp2);

								if (i) {
									lpwStrTemp = new(std::nothrow) WCHAR[lenTmp + EXTRA_CHAR];

									if (lpwStrTemp) {
										_snwprintf(lpwStrTemp, lenTmp + EXTRA_CHAR, L",\"%s\"<%s>", (LPWSTR) bstrTemp1, (LPWSTR) bstrTemp2);
										wcscat_s(lpwRecipients, len, lpwStrTemp);
										delete [] lpwStrTemp;
									}
								}else{
									lpwStrTemp = new(std::nothrow) WCHAR[lenTmp + EXTRA_CHAR];

									if (lpwStrTemp) {
										_snwprintf(lpwStrTemp, lenTmp + EXTRA_CHAR, L"\"%s\"<%s>", (LPWSTR) bstrTemp1, (LPWSTR) bstrTemp2);
										wcscat_s(lpwRecipients, len, lpwStrTemp);
										delete [] lpwStrTemp;
									}
								}

								if (bstrTemp1)	
									SysFreeString(bstrTemp1);

								if (bstrTemp2)	
									SysFreeString(bstrTemp2);
							}						
							
							if (pIRecip) {
								pIRecip->Release();
								pIRecip = NULL;
							}
						} // end for

						calendar->SetRecipients(lpwRecipients);
					}
				}
			}

			if (pIRecipients) 
				pIRecipients->Release();
		}
	}

	// RECURRENCE
	if (SUCCEEDED(iAppointment->get_IsRecurring(&variantBoolTemp))) {
		if (variantBoolTemp == VARIANT_TRUE) {
			calendar->SetFlags( calendar->Flags() | FLAG_RECUR);

			RecurStruct* rStruct = calendar->GetRecurStruct();
			ZeroMemory(rStruct, sizeof(RecurStruct));
			IRecurrencePattern *pRecurrence;	

			if (SUCCEEDED(iAppointment->GetRecurrencePattern(&pRecurrence))) {
				// LONG
				if(SUCCEEDED(pRecurrence->get_RecurrenceType(&longTemp)))
					calendar->SetRecurrenceType(longTemp);

				if(SUCCEEDED(pRecurrence->get_Interval(&longTemp)))
					calendar->SetInterval(longTemp);

				if(SUCCEEDED(pRecurrence->get_MonthOfYear(&longTemp)))
					calendar->SetMonthOfYear(longTemp);

				if(SUCCEEDED(pRecurrence->get_DayOfMonth(&longTemp)))
					calendar->SetDayOfMonth(longTemp);

				if(SUCCEEDED(pRecurrence->get_DayOfWeekMask(&longTemp))) 
					calendar->SetDayOfWeekMask(longTemp);

				if(SUCCEEDED(pRecurrence->get_Instance(&longTemp)))
					calendar->SetInstance(longTemp);

				// DATE
				GETDATE(pRecurrence->get_PatternStartDate, calendar->SetPatternStartDate);

				// VARIANT BOOL
				if (SUCCEEDED(pRecurrence->get_NoEndDate(&variantBoolTemp))) {
					if (variantBoolTemp == VARIANT_TRUE) {
						calendar->SetFlags( calendar->Flags() | FLAG_RECUR_NoEndDate);
						GETDATE(pRecurrence->get_PatternEndDate, calendar->SetPatternEndDate);					
					}

					if (SUCCEEDED(pRecurrence->get_Occurrences(&longTemp)))
						calendar->SetOccurrences(longTemp);
				}
			}

			if (pRecurrence) 
				pRecurrence->Release();
		}
	}
}