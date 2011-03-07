#include "Factory.h"
#include "MapiRule.h"

extern int g_cServerLocks;

/**
 *	.ctor
 *	Initialize count ref.
 **/
CFactory::CFactory()
	:	m_cRef(1)
{
}

/**
 *	.dtor
 *	Default denstructor
 **/
CFactory::~CFactory()
{
}

HRESULT CFactory::QueryInterface(REFIID rif, void** ppobj)
{
	if (!ppobj)
		return E_INVALIDARG;

	*ppobj = NULL;

	if ((rif == IID_IUnknown) || (rif == IID_IClassFactory)) {
		*ppobj = (LPVOID) this;
	}

	if (*ppobj)
		((LPUNKNOWN)*ppobj)->AddRef();

	return (*ppobj) ? S_OK : E_NOINTERFACE;
}


/**
 *	AddRef
 *	COM reference counting
 *
 *	Implements IUnknown::AddRef by adding 1 to the object's reference count
 **/
ULONG CFactory::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

/**
 *	Release
 *	COM reference counting
 **/
ULONG CFactory::Release()
{
	InterlockedDecrement(&m_cRef);

	if (m_cRef == 0) {
		delete this;
		return 0;
	}

	return m_cRef;
}

STDMETHODIMP CFactory::CreateInstance(IUnknown *pUnknownOuter, REFIID iid, LPVOID *ppv)
{
	CMailRuleClient *pClient = NULL;

	if (pUnknownOuter)
		return CLASS_E_NOAGGREGATION;

	pClient = new CMailRuleClient();

	if (pClient == NULL)
		return E_OUTOFMEMORY;

	return pClient->QueryInterface(iid, ppv);
}

STDMETHODIMP CFactory::LockServer(BOOL bLock)
{
	if (bLock)
		g_cServerLocks++;
	else
		g_cServerLocks--;

	return S_OK;
}

