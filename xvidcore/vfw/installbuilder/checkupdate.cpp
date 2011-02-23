#include <windows.h>

int CALLBACK WinMain(
  __in  HINSTANCE hInstance,
  __in  HINSTANCE hPrevInstance,
  __in  LPSTR lpCmdLine,
  __in  int nCmdShow
)
{
	CHAR path[MAX_PATH]; 
	DWORD lExitCode = 0;

	GetModuleFileNameA(NULL, path, ARRAYSIZE(path)); 
	path[strlen(path)-15]='\0';

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	SHELLEXECUTEINFOA ShExecInfo = {0}; 
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO); 
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS; 
	ShExecInfo.hwnd = NULL; 
	ShExecInfo.lpVerb = NULL; 
	ShExecInfo.lpFile = strcat(path, "autoupdate-windows.exe");       
	ShExecInfo.lpParameters = "   --mode unattended";    
	ShExecInfo.lpDirectory = NULL; 
	ShExecInfo.nShow = SW_SHOW; 
	ShExecInfo.hInstApp = NULL;  
	
	ShellExecuteExA(&ShExecInfo); 
	WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
    GetExitCodeProcess(ShExecInfo.hProcess, &lExitCode);

	if (lExitCode == 0) { 
		MessageBox(0, L"A new version of Xvid is available!\nYou can run the Updater now to install it.", L"Xvid AutoUpdate", MB_ICONINFORMATION);
		ShellExecuteA(0, "open", path, 0,0,SW_SHOWNORMAL);
	}

	return 0;
}

