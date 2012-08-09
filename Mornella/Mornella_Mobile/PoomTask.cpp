#include "PoomCommon.h"
#include "PoomTask.h"

CPoomTask::CPoomTask()
:	_pHeader(NULL), _pRecur(NULL),
	_dwFlags(FLAG_ISTASK), _lSensitivity(0), _lImportance(0),
	_lpwSubject(NULL), _lpwCategories(NULL), _lpwBody(NULL)
{
	_pHeader = (HeaderStruct*) LocalAlloc(LPTR, sizeof(HeaderStruct));
	ZeroMemory(&_ftDueDate, sizeof(FILETIME));
	ZeroMemory(&_ftDateCompleted, sizeof(FILETIME));
	ZeroMemory(&_ftStartDate, sizeof(FILETIME));
}

CPoomTask::~CPoomTask(void)
{
	if(_pHeader) LocalFree(_pHeader);

	if(_lpwSubject)
		delete [] _lpwSubject;
	if(_lpwCategories)
		delete [] _lpwCategories;
	if(_lpwBody)
		delete [] _lpwBody;

	if(_pRecur)
		delete _pRecur;
}