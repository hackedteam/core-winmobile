#include <sms.h>
#include "SmsMessage.h"

static LPCTSTR ptsMessageProtocol = _T("Microsoft Text SMS Protocol");

Sms::Sms(void)
{
	SmsOpen(ptsMessageProtocol, SMS_MODE_SEND, &this->_smsHandle, &_hMsgAvailableEvent);
}

Sms::~Sms(void)
{
	SmsClose(_smsHandle);
}

BOOL Sms::SendMessage(LPWSTR szDestination, const LPWSTR szMessage) {
	SMS_MESSAGE_ID messageId;
	SMS_ADDRESS smsDestAddr;

	ZeroMemory(&smsDestAddr, sizeof(SMS_ADDRESS));

	smsDestAddr.smsatAddressType = SMSAT_INTERNATIONAL;
	wcscpy(smsDestAddr.ptsAddress, szDestination);


	TEXT_PROVIDER_SPECIFIC_DATA tpsd;

	ZeroMemory(&tpsd, sizeof(tpsd));

	tpsd.dwMessageOptions = PS_MESSAGE_OPTION_NONE;
	tpsd.psMessageClass = PS_MESSAGE_CLASS1;
	tpsd.psReplaceOption = PSRO_NONE;


	HRESULT hr = 
		SmsSendMessage(_smsHandle, NULL, &smsDestAddr, NULL, (PBYTE) szMessage, _tcslen(szMessage) * sizeof(TCHAR), (PBYTE) &tpsd, sizeof(TEXT_PROVIDER_SPECIFIC_DATA), SMSDE_GSM, SMS_OPTION_DELIVERY_NONE, &messageId);

	return (hr == S_OK) ? TRUE : FALSE;
}
