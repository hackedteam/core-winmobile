#include "Common.h"
#include <string>
#include <exception>
using namespace std;

#ifndef __Timer_h__
#define __Timer_h__

#include "Status.h"

class Status;
class Timer;

class Timer
{
	private: list<TimerStruct>* timerList;
	/**
	 * Oggetto status che serve per accedere alla tabella degli eventi.
	 */
	private: Status statusObj;

	private: void MiniSleep(UINT delay);

	/**
	 * Svuota la lista dei timer. Torna TRUE se la funzione e' andata a buon fine,
	 * FALSE altrimenti.
	 */
	private: BOOL Clear();

	/**
	 * Aggiunge un timer alla lista o ne aggiorna lo stato se il timer gia' esiste. Torna TRUE se il timer e' stato
	 * aggiunto/updatato, FALSE altrimenti.
	 */
	public: BOOL AddTimer(UINT timeStruct);

	/**
	 * Loop infinito che effettua ciclo per il timer, esce con una ExitThread() quando viene richiesto lo stop
	 * dell'agente.
	 */
	public: void Run();
};

#endif
