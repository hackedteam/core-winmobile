#pragma once

#include "PoomCommon.h"

class CPoomTask
{
private:
	HeaderStruct *_pHeader;

	DWORD		_dwFlags;
	FILETIME	_ftDueDate;
	FILETIME	_ftDateCompleted;
	FILETIME	_ftStartDate;

	LONG		_lSensitivity;
	LONG		_lImportance;
	
	// Dynamic entries
	RecurStruct *_pRecur;

	LPWSTR	_lpwSubject;
	LPWSTR	_lpwCategories;
	LPWSTR	_lpwBody;

public:
	CPoomTask(void);
	~CPoomTask(void);

	HeaderStruct* Header(void) { return _pHeader; }
	void SetHeader(HeaderStruct *pHeader) { _pHeader = pHeader; }

	// FLAGS
	void SetFlags(DWORD dwFlags){ _dwFlags = dwFlags; }
	DWORD Flags(){ return _dwFlags; }

	// DATE
	void SetDueDate(FILETIME *ftDueDate){ _ftDueDate = *ftDueDate; }
	FILETIME DueDate(){ return _ftDueDate; }

	void SetDateCompleted(FILETIME *ftDateCompleted){ _ftDateCompleted = *ftDateCompleted; }
	FILETIME DateCompleted(){ return _ftDateCompleted; }

	void SetStartDate(FILETIME *ftStartDate){ _ftStartDate = *ftStartDate; }
	FILETIME StartDate(){ return _ftStartDate; }

	// LONG
	void SetSensitivity(LONG *lSensitivity){ _lSensitivity = *lSensitivity; }
	LONG Sensitivity(){ return _lSensitivity; }

	void SetImportance(LONG *lImportance){ _lImportance = *lImportance; }
	LONG Importance(){ return _lImportance; }

	// STRINGS
	void SetSubject(LPWSTR lpwSubject){ _lpwSubject = lpwSubject; }
	LPWSTR Subject(){ return _lpwSubject; }

	void SetCategories(LPWSTR lpwCategories){ _lpwCategories = lpwCategories; }
	LPWSTR Categories(){ return _lpwCategories; }

	void SetBody(LPWSTR lpwBody){ _lpwBody = lpwBody; }
	LPWSTR Body(){ return _lpwBody; }

	// Additional Data
		// Recurrence 
		RecurStruct* GetRecurStruct(void) { return (_pRecur != NULL) ? _pRecur : _pRecur = new RecurStruct;}

		// LONG
		void SetInterval (LONG lInterval){ _pRecur->lInterval = lInterval; }
		LONG Interval(){ return _pRecur->lInterval; }

		void SetMonthOfYear (LONG lMonthOfYear){ _pRecur->lInterval = lMonthOfYear; }
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
		FILETIME PatternStartDate(){ return _pRecur->ftPatternStartDate;}

		void SetPatternEndDate(FILETIME *ftPatternEndDate){ _pRecur->ftPatternEndDate = *ftPatternEndDate; }
		FILETIME PatternEndDate(){ return _pRecur->ftPatternEndDate;}
};