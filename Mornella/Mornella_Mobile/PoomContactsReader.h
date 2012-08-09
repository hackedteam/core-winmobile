#pragma once

#include "PoomMan.h"

class CPoomContactsReader :
	public IPoomFolderReader
{
private:
	void Parse(IContact *iContact, CPoomContact *contact);
	IPOutlookItemCollection* _items;

public:
	CPoomContactsReader(IFolder* pIFolder);
	~CPoomContactsReader(void);

	int Count();

	HRESULT	Get(int i, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *uBufLen);
	HRESULT	GetOne(IPOutlookApp* _pIPoomApp, LONG lOid, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT* puBufLen);
};