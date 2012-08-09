#pragma once

#include "PoomCommon.h"

class CPoomCalendar
{
private:
	HeaderStruct * _pHeader;

	DWORD		_dwFlags;
	FILETIME	_ftStartDate;
	FILETIME	_ftEndDate;
	LONG		_lSensitivity;
	LONG		_lBusyStatus;
	LONG		_lDuration;
	LONG		_lMeetingStatus;

	// Dynamic entries
	RecurStruct * _pRecur;

	LPWSTR	_lpwSubject;
	LPWSTR	_lpwCategories;
	LPWSTR	_lpwBody;
	
	LPWSTR  _lpwRecipients;	// solo per Appointment

	LPWSTR	_lpwLocation;

public:
	CPoomCalendar(void);
	~CPoomCalendar(void);

	HeaderStruct * Header(void) { return _pHeader; }
	void SetHeader(HeaderStruct *pHeader) { _pHeader = pHeader; }

	// FLAGS
	void SetFlags(DWORD dwFlags){ _dwFlags = dwFlags; }
	DWORD Flags(){ return _dwFlags; }

	// DATE
	void SetStartDate(FILETIME *ftStartDate){ _ftStartDate = *ftStartDate; }
	FILETIME StartDate(){ return _ftStartDate; }
	
	void SetEndDate(FILETIME *ftEndDate){ _ftEndDate = *ftEndDate; }
	FILETIME EndDate(){ return _ftEndDate; }

	// LONG
	void SetSensitivity(LONG *lSensitivity){ _lSensitivity = *lSensitivity; }
	LONG Sensitivity(){ return _lSensitivity; }
	
	void SetBusyStatus(LONG *lBusyStatus){ _lBusyStatus = *lBusyStatus; }
	LONG BusyStatus(){ return _lBusyStatus; }
	
	void SetDuration(LONG *lDuration){ _lDuration = *lDuration; }
	LONG Duration(){ return _lDuration; }
	
	void SetMeetingStatus(LONG *lMeetingStatus){ _lMeetingStatus = *lMeetingStatus; }
	LONG MeetingStatus(){ return _lMeetingStatus; }
 
	// STRINGS
	void SetSubject(LPWSTR lpwSubject){ _lpwSubject = lpwSubject; }
	LPWSTR Subject(){ return _lpwSubject; }

	void SetCategories(LPWSTR lpwCategories){ _lpwCategories = lpwCategories; }
	LPWSTR Categories(){ return _lpwCategories; }

	void SetBody(LPWSTR lpwBody){ _lpwBody = lpwBody; }
	LPWSTR Body(){ return _lpwBody; }

	void SetLocation (LPWSTR lpwLocation){ _lpwLocation = lpwLocation; }
	LPWSTR Location(){ return _lpwLocation; }

	// Additional Data
		// Recurrence 
		RecurStruct* GetRecurStruct(void) { return (_pRecur != NULL) ? _pRecur : _pRecur = new RecurStruct; }
		//////////////////////////	

		// Recipient
		void SetRecipients(LPWSTR lpwRecipients){ _lpwRecipients = lpwRecipients; }
		LPWSTR Recipients(){ return _lpwRecipients; }
		// LONG
			void SetInterval (LONG lInterval){ _pRecur->lInterval = lInterval; }
			LONG Interval(){ return _pRecur->lInterval; }

			void SetMonthOfYear (LONG lMonthOfYear){ _pRecur->lMonthOfYear = lMonthOfYear; }
			LONG MonthOfYear(){ return _pRecur->lMonthOfYear; }

			void SetDayOfMonth (LONG lDayOfMonth){ _pRecur->lDayOfMonth = lDayOfMonth; }
			LONG DayOfMonth(){ return _pRecur->lDayOfMonth; }

			void SetDayOfWeekMask (LONG lDayOfWeekMask){ _pRecur->lDayOfWeekMask = lDayOfWeekMask; }
			LONG DayOfWeekMask(){ return _pRecur->lDayOfWeekMask; }

			void SetInstance (LONG lInstance){ _pRecur->lInstance = lInstance; }
			LONG Instance(){ return _pRecur->lInstance; }

			void SetRecurrenceType (LONG lRecurrenceType){ _pRecur->lRecurrenceType = lRecurrenceType; }
			LONG RecurrenceType(){ return _pRecur->lRecurrenceType; }

			void SetOccurrences(LONG lOccurrences){ _pRecur->lOccurrences = lOccurrences; };
			LONG Occurrences(){ return _pRecur->lOccurrences; }

		// FILETIME 
			void SetPatternStartDate (FILETIME *ftPatternStartDate){ _pRecur->ftPatternStartDate = *ftPatternStartDate; }
			FILETIME PatternStartDate(){ return _pRecur->ftPatternStartDate; }

			void SetPatternEndDate(FILETIME *ftPatternEndDate){ _pRecur->ftPatternEndDate = *ftPatternEndDate; }
			FILETIME PatternEndDate(){ return _pRecur->ftPatternEndDate; }
};