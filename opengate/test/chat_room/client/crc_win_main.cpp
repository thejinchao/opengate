#include "cr_stdafx.h"
#include "crc_chat_dialog.h"

CAppModule _AppModule;

//--------------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int nCmdShow)
{
	::InitCommonControls();
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	_AppModule.Init(NULL, hInstance);

	// Show main dialog
	CChatDialog dlgChat;
	dlgChat.DoModal();

	// Close WTL app module
	_AppModule.Term();

	return 0;
}

