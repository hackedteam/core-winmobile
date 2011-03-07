#ifndef __FACTORY_H__
#define __FACTORY_H__

#define INITGUID
#include <windows.h>
#include <cemapi.h>

//*********************************************************************
//Class CFactory - Class factory for CMailRuleClient objects
//
//Inheritance:
//	IClassFactory
//	IUnknown
//
//Purpose:
//	This class provides a standard COM class factory implementation
//	for CMailRuleClient
//
//**********************************************************************
class CFactory : public IClassFactory
{
	private:
		long m_cRef;

	public:
		CFactory();
		~CFactory();

		//IUnknown
		STDMETHOD (QueryInterface)(REFIID iid, LPVOID *ppv);
		STDMETHOD_(ULONG, AddRef)();
		STDMETHOD_(ULONG, Release)();

		//IClassFactory Interfaces
		STDMETHOD (CreateInstance)(IUnknown *pUnknownOuter, const IID& iid, LPVOID *ppv);
		STDMETHOD (LockServer)(BOOL bLock);
};

#endif
