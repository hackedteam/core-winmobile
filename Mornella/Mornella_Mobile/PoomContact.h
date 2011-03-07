#pragma once

#include "PoomCommon.h"

class CPoomContact
{
private:
	HeaderStruct *_pHeader;
	ContactMapType	*_pContactMap;

public:
	CPoomContact();
	virtual ~CPoomContact(void);

	HeaderStruct* Header(void) { return _pHeader; }
	void SetHeader(HeaderStruct *pHeader){ _pHeader = pHeader; }

	ContactMapType* Map(void){ return _pContactMap; }
};