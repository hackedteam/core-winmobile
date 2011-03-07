#pragma once
#include "PoomMan.h"

class CPoomContactsSIMReader :
	public IPoomFolderReader
{
private:
	HSIM	m_hSim;
	BOOL	m_bInit;
	DWORD	m_dwTotal, m_dwUsed, m_dwOSMinVersion;
	SIMCAPS m_simCaps;

public:
	CPoomContactsSIMReader();
	~CPoomContactsSIMReader(void);
	
	BOOL	InitializeSIM();
	INT		Count();
	HRESULT	Get(INT i, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *uBufLen);
	HRESULT	GetOne(IPOutlookApp* _pIPoomApp, LONG lOid, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT* puBufLen){ return E_FAIL; }
	HRESULT Parse(SIMPHONEBOOKENTRY* lpsimEntry, CPoomContact *contact);
	HRESULT ParseEx(SIMPHONEBOOKENTRYEX* lpSimPBEx, CPoomContact *contact);
};