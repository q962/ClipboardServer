#include <windows.h>

int main( int argc, char** args )
{
	STARTUPINFO         si        = {};
	PROCESS_INFORMATION dns_sd_pi = {};

	ZeroMemory( &si, sizeof( si ) );
	si.cb          = sizeof( si );
	si.dwFlags     = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	wchar_t commandLine[] = L"bin/ClipboaredServer";
	if ( !CreateProcess( NULL,              //
	                     commandLine,       //
	                     NULL,              //
	                     NULL,              //
	                     FALSE,             //
	                     DETACHED_PROCESS,  //
	                     NULL,              //
	                     NULL,              //
	                     &si,               //
	                     &dns_sd_pi ) )     //
	{
		MessageBox( NULL, L"启动失败", L"提示", MB_OK | MB_ICONERROR );

		return -1;
	}

	return 0;
}
