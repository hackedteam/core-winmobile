#include "PoomCommon.h"
#include "PoomContact.h"

CPoomContact::CPoomContact()
:	_pHeader(NULL), _pContactMap(NULL)
{
	_pHeader = (HeaderStruct*) LocalAlloc(LPTR, sizeof(HeaderStruct));
	_pContactMap = new ContactMapType;
}

CPoomContact::~CPoomContact(void)
{
	if(_pHeader) LocalFree(_pHeader);

	if(_pContactMap){
		for(ContactMapType::iterator it = _pContactMap->begin(); it != _pContactMap->end(); ){

			if(it->second) delete [] it->second;

			_pContactMap->erase(it++);
		}
		delete _pContactMap;
	}
}