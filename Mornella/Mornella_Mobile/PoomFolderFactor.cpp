#include "PoomMan.h"
#include "PoomCalendarReader.h"
#include "PoomContactsReader.h"
#include "PoomContactsSIMReader.h"
#include "PoomTaskReader.h"

IPoomFolderReader* CPoomFolderFactor::get(OlDefaultFolders folder, IFolder* pIFolder){
	
	IPoomFolderReader * reader = NULL;

	switch(folder){
		case olFolderCalendar:
			reader = new(nothrow) CPoomCalendarReader(pIFolder);
			reader->SetType(LOGTYPE_CALENDAR);
			break;
		case olFolderContacts:
			reader = new(nothrow) CPoomContactsReader(pIFolder);
			reader->SetType(LOGTYPE_ADDRESSBOOK);
			break;
		case olFolderContactsSim:
			reader = new(nothrow) CPoomContactsSIMReader();
			reader->SetType(LOGTYPE_ADDRESSBOOK);
			
			if(FALSE == ((CPoomContactsSIMReader*)reader)->InitializeSIM()) {
				delete reader;
				reader = NULL;
			}
			break;
		case olFolderTasks:	
			reader = new(nothrow) CPoomTaskReader(pIFolder);
			reader->SetType(LOGTYPE_TASK);
			break;
		case olFolderCities:
			break;
		case olFolderInfrared:
			break;
		default:
			break;
	}

	if (reader == NULL)	{
		DBG_TRACE(L"Debug - PoomFolderFactor.cpp - New reader FAILED ", 5, FALSE);
	}

	return reader;
}