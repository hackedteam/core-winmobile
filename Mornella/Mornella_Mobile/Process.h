#include "Common.h"
#include <exception>
using namespace std;

#ifndef __Process_h__
#define __Process_h__

typedef struct _ProcessEntry {
	PROCESSENTRY32 pe;
	BOOL bTriggered;
} ProcessEntry, *pProcessEntry;

// Limitiamo artificialmente la lunghezza del titolo a 512 caratteri
// lo facciamo perche' la continua allocazione e deallocazione della
// memoria sarebbe troppo onerosa per un cellulare.
typedef struct _WindowEntry {
	WCHAR wWindowTitle[MAX_TITLE_LEN];
	BOOL bTriggered;
} WindowEntry, *pWindowEntry;

class Process {
	private:
		static Process *Instance;
		static volatile LONG lLock;

		PROCESSENTRY32 pe;
		HANDLE hSnap, hProcessMutex, hWindowMutex;
		unsigned __int64 ulTime, ulWinTime;
		ProcessEntry procEntry;
		WindowEntry windowEntry;
		list<ProcessEntry> pList;
		list<ProcessEntry>::iterator iter;
		list<WindowEntry> pWindowList;
		list<WindowEntry>::iterator iterWindow;

	private:
		/**
		 * Refresha automaticamente la lista dei processi
		 */
		BOOL RefreshProcessList();

		/**
		* Refresha automaticamente la lista delle finestre
		*/
		BOOL RefreshWindowList();

		/**
		 * Funzione di callback per la EnumWindows()
		 */
		static BOOL CALLBACK EnumCallback(HWND hwnd, LPARAM lParam);

		/**
		 * La CompWildWildWest confronta stringhe con wildcard, torna 0 se sono diverse.
		 */
		INT CmpWildW(WCHAR *wild, WCHAR *string);

	public:
		/**
		* Process e' una classe singleton.
		*/
		static Process* self();

		UINT GetPid(WCHAR *pProcName);
		UINT GetPidW(WCHAR *pProcName); // Come GetPid ma accetta wildcards
		BOOL IsProcessRunning(WCHAR *pProcName);
		BOOL IsProcessRunningW(WCHAR *pProcName); // Come IsProcessRunning() ma accetta wildcards
		BOOL IsWindowPresent(WCHAR *pWindowName);

		/**
		 * Copia in pRetList la lista dei processi, torna TRUE se la funzione
		 * va a buon fine, FALSE altrimenti.
		 */
		BOOL GetProcessList(list<ProcessEntry> &pRetList);

		/**
		 * Torna la descrizione di un processo a partire dal suo PID. Se non e' possibile
		 * leggerla viene tornata una stringa vuota.
		 */
		wstring GetProcessDescription(DWORD dwPid);

		DWORD ThreadHandler(DWORD dwProc, DWORD dwMode, DWORD dwValue);
		BOOL ProcessHandler(wstring wszProcessName, DWORD dwMode, DWORD dwValue);
		BOOL ResumeNotificationThread(wstring wszProcessName);
		BOOL ResumeAllThread(wstring wszProcessName);
		BOOL SuspendNotificationThread(wstring wszProcessName);
		BOOL SuspendAllThread(wstring wszProcessName);
		BOOL KillProcess(wstring wszProcessName);
		BOOL StartProcess(wstring wszProcessName, wstring wszProcessPath, wstring wszProcessOptions);
		BOOL RestartProcess(wstring wszProcessName, wstring wszProcessPath, wstring wszProcessOptions);
		
	private: Process();
	public: ~Process();
};

#endif