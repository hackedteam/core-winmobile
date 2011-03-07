/**
 *	H4CE
 *	tmail Agent
 *	2008.06
 **/

#define INITGUID


#include <windows.h>
#include <sms.h>
#include <cemapi.h>

/**
 *	WinMain entry point
 **/
/*
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	return 0;
}

int unregTMail()
{
	SmsClearMessageNotification(SMS_MSGTYPE_NOTIFICATION);

	return 0;
}*/

/*int regTMail(TCHAR* param)
{
	SMSREGISTRATIONDATA regData;

	memset(&regData, 0x00, sizeof(regData));

	regData.cbSize = sizeof(regData);
	
#ifdef UNICODE
	wcscpy(regData.tszAppName, param);
	wcscpy(regData.tszParams, TEXT("-attach"));
	wcscpy(regData.tszProtocolName, SMS_MSGTYPE_NOTIFICATION);
#else
	strcpy(regData.tszAppName, param);
	strcpy(regData.tszParams, "-attach");
	strcpy(regData.tszProtocolName, SMS_MSGTYPE_NOTIFICATION);
#endif

	unregTMail();	// un-reg. application!

	SmsSetMessageNotification(&regData);

	return 0;
}

int attachTMail()
{
	return 0;
}
*/
/**
 *	main
 **/
/*
int main(int argc, TCHAR* argv[])
{
	if (argc == 2) {	// require 2 parameters!
		bool bRequireRegister = false, bRequireAttach;

#ifdef UNICODE
		bRequireRegister = (wcscmp(argv[1], TEXT("-register")) == 0);
#else
		bRequireRegister = (strcmp(argv[1], "-register") == 0);
#endif

	
#ifdef UNICODE
		bRequireAttach = (wcscmp(argv[1], TEXT("-attach")) == 0);
#else
		bRequireAttach = (strcmp(argv[1], "-attach") == 0);
#endif
	
		if (bRequireRegister)
			return regTMail(argv[0]);

		if (bRequireAttach)
			return attachTMail();
	}

	return 0;
}
*/