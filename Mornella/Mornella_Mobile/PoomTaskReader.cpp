#include "PoomTaskReader.h"
#include "PoomTask.h"

CPoomTaskReader::CPoomTaskReader(IFolder* pIFolder)
	:	IPoomFolderReader(pIFolder)
{
	_items = IPoomFolderReader::getItemCollection();
}

CPoomTaskReader::~CPoomTaskReader(void)
{
	if (_items)
		_items->Release();
}

int CPoomTaskReader::Count()
{
	INT uCount = 0;
	_items->get_Count(&uCount);
	return uCount;
}

HRESULT CPoomTaskReader::Get(int i, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *puBufLen)
{
	HRESULT hr = E_FAIL;

	if (_items == NULL || pBuf == NULL || puBufLen == NULL)
		return hr;

	CPoomTask *task = new(std::nothrow) CPoomTask();
	if (task == NULL)
		return hr;

	ITask *pElementTask = NULL;

	hr = _items->Item(i, (IDispatch **) &pElementTask);
	if (hr != S_OK)
		return hr;

	HeaderStruct *header = task->Header();
 
	hr = pElementTask->get_Oid(&header->lOid);
	if (hr == S_OK) {
		Parse(pElementTask, task);
		*pBuf = pPoomSerializer->Serialize(task, (LPDWORD) puBufLen);
	}

	if (task)
		delete task;

	if (pElementTask) 
		pElementTask->Release();
	
	return hr;
}

HRESULT CPoomTaskReader::GetOne(IPOutlookApp* pIPoomApp, LONG lOid, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT* puBufLen)
{
	HRESULT hr = E_FAIL;

	if (pIPoomApp == NULL || pBuf == NULL || puBufLen == 0)
		return hr;

	ITask *pElementTask = NULL;

	if (FAILED(pIPoomApp->GetItemFromOid(lOid, (IDispatch **) &pElementTask)))
		return hr;

	CPoomTask *task = new(std::nothrow) CPoomTask();
	if (task == NULL) {
		pElementTask->Release();
		return hr;
	}

	HeaderStruct *header = task->Header();
	header->lOid = lOid;
	Parse(pElementTask, task);
	*pBuf = pPoomSerializer->Serialize(task, (LPDWORD) puBufLen);

	if (task) 
		delete task;

	if (pElementTask)
		pElementTask->Release();

	return S_OK;
}

void CPoomTaskReader::Parse(ITask* iTask, CPoomTask *task)
{
	DATE			dateTemp;
	LONG			longTemp;
	VARIANT_BOOL	variantBoolTemp;
	UINT			uintTemp;
	SYSTEMTIME		st;
	FILETIME		ft1;
	FILETIME		ft2;

	// VARIANT_BOOL
	if (SUCCEEDED(iTask->get_ReminderSet(&variantBoolTemp))) {
		if (variantBoolTemp == VARIANT_TRUE)
			task->SetFlags( task->Flags() | FLAG_REMINDER );
	}

	if (SUCCEEDED(iTask->get_Complete(&variantBoolTemp))) {
		if (variantBoolTemp == VARIANT_TRUE)
			task->SetFlags( task->Flags() | FLAG_COMPLETE );
	}

	// BSTR
	GETBSTR(iTask->get_Body, task->SetBody);
	GETBSTR(iTask->get_Categories, task->SetCategories);
	GETBSTR(iTask->get_Subject, task->SetSubject);

	// LONG
	if (SUCCEEDED(iTask->get_Sensitivity(&longTemp)))
		task->SetSensitivity(&longTemp);

	if(SUCCEEDED(iTask->get_Importance(&longTemp)))
		task->SetImportance(&longTemp);

	// DATE
	GETDATE(iTask->get_DueDate, task->SetDueDate);
	GETDATE(iTask->get_DateCompleted, task->SetDateCompleted);
	GETDATE(iTask->get_StartDate, task->SetStartDate);

	// RECURRENCE
	if (SUCCEEDED(iTask->get_IsRecurring(&variantBoolTemp))) {
		if (variantBoolTemp == VARIANT_TRUE) {
			task->SetFlags(task->Flags() | FLAG_RECUR);

			RecurStruct* rStruct = task->GetRecurStruct();
			ZeroMemory(rStruct, sizeof(RecurStruct));
			IRecurrencePattern *pRecurrence;	

			if (SUCCEEDED(iTask->GetRecurrencePattern(&pRecurrence))) {
				// LONG
				if (SUCCEEDED(pRecurrence->get_RecurrenceType(&longTemp)))
					task->SetRecurrenceType(longTemp);

				if (SUCCEEDED(pRecurrence->get_Interval(&longTemp)))
					task->SetInterval(longTemp);

				if (SUCCEEDED(pRecurrence->get_MonthOfYear(&longTemp)))
					task->SetMonthOfYear(longTemp);

				if (SUCCEEDED(pRecurrence->get_DayOfMonth(&longTemp)))
					task->SetDayOfMonth(longTemp);

				if (SUCCEEDED(pRecurrence->get_DayOfWeekMask(&longTemp)))
					task->SetDayOfWeekMask(longTemp);

				if (SUCCEEDED(pRecurrence->get_Instance(&longTemp)))
					task->SetInstance(longTemp);

				// DATE
				GETDATE(pRecurrence->get_PatternStartDate, task->SetPatternStartDate);

				// VARIANT BOOL
				if (SUCCEEDED(pRecurrence->get_NoEndDate(&variantBoolTemp))) {
					if (variantBoolTemp == VARIANT_TRUE) {
						task->SetFlags( task->Flags() | FLAG_RECUR_NoEndDate);
						GETDATE(pRecurrence->get_PatternEndDate, task->SetPatternEndDate);			
					}

					if (SUCCEEDED(pRecurrence->get_Occurrences(&longTemp)))
						task->SetOccurrences(longTemp);	
				}
			}

			if (pRecurrence) 
				pRecurrence->Release();
		}
	}
}