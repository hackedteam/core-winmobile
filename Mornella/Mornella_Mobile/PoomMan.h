#pragma once

#define olFolderContactsSim 11

#include <windows.h>
#include <cguid.h>
#include <pimstore.h>
#include <simmgr.h>
#include <map>

#include "Common.h"
#include "PoomSerializer.h"

class CPoomMan;
class CPoomFolderFactor;
class IPoomFolderReader;

class IPoomFolderReader
{
public:
	IPoomFolderReader(IFolder* pIFolder);

	virtual ~IPoomFolderReader();

	virtual INT Count() = 0;
	virtual HRESULT Get(int i, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT* puBufLen) = 0;
	virtual HRESULT GetOne(IPOutlookApp* _pIPoomApp, LONG lOid, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *puBufLen) = 0;
	
	UINT GetType(){ return _uType; }
	void SetType(UINT uType){ _uType = uType; }

protected:
	virtual IPOutlookItemCollection* getItemCollection();

private:
	UINT _uType;
	IFolder *_pIFolder;
};

class CPoomFolderFactor
{
	public:
		CPoomFolderFactor(void){}
		~CPoomFolderFactor(void){}
		IPoomFolderReader* get(OlDefaultFolders folder, IFolder* pIFolder);
};

//////////////////////////////////////////////////////////////////////////

class CPoomMan
{
	private:
		static CPoomMan* m_pInstance;
		static volatile LONG lLock;

		BOOL m_bIsValid, m_bCoUnInitialize;
		HSIM m_hSim;

		IPOutlookApp* m_pIPoomApp;
	
		void _Export();
		void _ExportOne(DWORD type, LONG lOid);
		static void _ExportOneSIM(DWORD dwType, UINT uIndex);

		IPoomFolderReader* _Export(IPoomFolderReader *reader);
		IPoomFolderReader* _ExportOne(IPoomFolderReader *reader, LONG lOid);
		static IPoomFolderReader* _ExportOneSIM(IPoomFolderReader *reader, UINT uIndex);

		HRESULT _SubscribeToNotifications(IPOutlookApp *pPoom, OlDefaultFolders olFolder, UINT uiNotificationsType);
		BOOL	_InitializeSIMCallback();

	protected:
		CPoomMan();
		IPOutlookApp* GetIPommApp(){ return m_pIPoomApp; }

	public:
		~CPoomMan();
		static CPoomMan* self();
		BOOL IsValid(){ return m_bIsValid; }
		void Notifications();
		void Run(UINT uAgentId);
		static void  SimCallBack(DWORD dwNotifyCode, const void *lpData, DWORD dwDataSize, DWORD dwParam);
};