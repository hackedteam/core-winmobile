#include "PoomCommon.h"
#include "PoomCalendar.h"

CPoomCalendar::CPoomCalendar()
:	_pHeader(NULL), _pRecur(NULL),
	_dwFlags(0), _lSensitivity(0), _lBusyStatus(0), _lDuration(0), _lMeetingStatus(0),
	_lpwSubject(NULL), _lpwCategories(NULL), _lpwBody(NULL), _lpwLocation(NULL), _lpwRecipients(NULL)
{
	_pHeader = (HeaderStruct*) LocalAlloc(LPTR, sizeof(HeaderStruct));
	ZeroMemory(&_ftStartDate, sizeof(FILETIME));
	ZeroMemory(&_ftEndDate, sizeof(FILETIME));
}

CPoomCalendar::~CPoomCalendar(void)
{
	if(_pHeader) LocalFree(_pHeader);
	
	if(_lpwSubject)
		delete [] _lpwSubject;
	if(_lpwCategories)
		delete [] _lpwCategories;
	if(_lpwBody)
		delete [] _lpwBody;
	if(_lpwLocation)
		delete [] _lpwLocation;

	if(_lpwRecipients)
		delete [] _lpwRecipients;
	if(_pRecur)
		delete _pRecur;
}