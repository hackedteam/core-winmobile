#pragma once

#include <windows.h>
#include <list>
#include <new>
#include <map>

using namespace std;

#define	POOM_V1_0_PROTO	0x01000000

#define FLAG_REMINDER			0x00000001
#define FLAG_COMPLETE			0x00000002
#define FLAG_TEAMTASK			0x00000004
#define FLAG_RECUR				0x00000008
#define FLAG_RECUR_NoEndDate	0x00000010
#define FLAG_MEETING			0x00000020
#define FLAG_ALLDAY				0x00000040
#define FLAG_ISTASK				0x00000080

enum ObjectTaskTypes{
	POOM_TYPE_MASK	= 0x00FFFFFF,

	POOM_STRING_SUBJECT		=	0x01000000, 
	POOM_STRING_CATEGORIES	=	0x02000000,
	POOM_STRING_BODY		=	0x04000000,
	POOM_STRING_RECIPIENTS	=	0x08000000,
	POOM_STRING_LOCATION	=	0x10000000,

	POOM_OBJECT_RECUR		=	0x80000000
};

typedef struct _TaskRecur{
	LONG lRecurrenceType;
	LONG lInterval;
	LONG lMonthOfYear;
	LONG lDayOfMonth;
	LONG lDayOfWeekMask;
	LONG lInstance;
	LONG lOccurrences;
	FILETIME	ftPatternStartDate;
	FILETIME	ftPatternEndDate;
} RecurStruct, *pRecurStruct;

#define SETLONG(x)	lTmp = x();	\
	CopyMemory(pPtr, &lTmp, sizeof(LONG));	pPtr += sizeof(LONG); \

#define GETBSTR(get, set)								\
	do{													\
		BSTR bstrTemp;									\
		LPWSTR lpwStrTemp;								\
		if(SUCCEEDED(get(&bstrTemp))){					\
			uintTemp = SysStringLen(bstrTemp);			\
			if(uintTemp > 0){							\
				lpwStrTemp = new wchar_t[uintTemp+1];	\
				wcscpy(lpwStrTemp, bstrTemp);			\
				set(lpwStrTemp);						\
			}											\
			if(bstrTemp){								\
				SysFreeString(bstrTemp);				\
				bstrTemp = NULL;						\
			}											\
		}												\
	}while(0)

#define GETDATE(func1, func2)							\
	do{													\
		if(SUCCEEDED(func1(&dateTemp))){				\
			ZeroMemory(&st, sizeof(SYSTEMTIME));		\
			ZeroMemory(&ft1, sizeof(FILETIME));			\
			ZeroMemory(&ft2, sizeof(FILETIME));			\
			VariantTimeToSystemTime(dateTemp, &st);		\
			SystemTimeToFileTime(&st, &ft1);			\
			LocalFileTimeToFileTime(&ft1, &ft2);		\
			func2(&ft2);								\
		}												\
	}while(0)

enum e_contactEntry{
	FirstName = 0x1,	LastName = 0x2,				CompanyName	= 0x3,				BusinessFaxNumber = 0x4,
	Department = 0x5,	Email1Address = 0x6,		MobileTelephoneNumber = 0x7,	OfficeLocation = 0x8, 
	PagerNumber = 0x9,	BusinessTelephoneNumber = 0xA, JobTitle = 0xB,				HomeTelephoneNumber = 0xC,
	Email2Address = 0xD,Spouse = 0xE,				Email3Address  = 0xF,			Home2TelephoneNumber = 0x10,

	HomeFaxNumber = 0x11,	CarTelephoneNumber = 0x12,	AssistantName = 0x13,	AssistantTelephoneNumber = 0x14,
	Children = 0x15,			Categories = 0x16,		WebPage = 0x17,			Business2TelephoneNumber = 0x18,
	RadioTelephoneNumber = 0x19,FileAs = 0x1A,			YomiCompanyName = 0x1B,	YomiFirstName = 0x1C,
	YomiLastName = 0x1D,		Title = 0x1E,			MiddleName = 0x1F,		Suffix = 0x20,

	HomeAddressStreet = 0x21,	HomeAddressCity = 0x22,			HomeAddressState = 0x23,	HomeAddressPostalCode = 0x24,	
	HomeAddressCountry = 0x25,	OtherAddressStreet = 0x26,		OtherAddressCity = 0x27,	OtherAddressPostalCode = 0x28,
	OtherAddressCountry = 0x29, BusinessAddressStreet = 0x2A,	 BusinessAddressCity = 0x2B,BusinessAddressState = 0x2C, 
	BusinessAddressPostalCode = 0x2D, BusinessAddressCountry = 0x2E, OtherAddressState = 0x2F, Body = 0x30,

	// NB: Birthday e Anniversary sono dei FILETIME messi a stringa!!!!!
	Birthday = 0x31,	Anniversary = 0x32,

	SkypeIM = 0x33
};

typedef map<e_contactEntry, LPWSTR> ContactMapType;

typedef struct _Header{
	DWORD		dwSize;
	DWORD		dwVersion;
	LONG		lOid;
} HeaderStruct, *pHeaderStruct;
