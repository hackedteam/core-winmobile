#pragma once

#include "PoomMan.h"

class CPoomTaskReader :
	public IPoomFolderReader
{
private:
	void Parse(ITask* iTask, CPoomTask *task);
	IPOutlookItemCollection* _items;

public:
	CPoomTaskReader(IFolder* pIFolder);
	~CPoomTaskReader(void);

	int Count();
	HRESULT  Get(int i, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *uBufLen);
	HRESULT GetOne(IPOutlookApp* _pIPoomApp, LONG lOid, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT* puBufLen);
};