/*
	CExplorerBgToolRe.cpp
	Classe per l'interfaccia con la DLL ExploreBgToolRe.dll.
	Unicode esplicito.
	Luca Piergentili, 18/06/2026

	Vedi le note in CExplorerBgToolRe.h
*/
#include "pragma.h"
#include "env.h"
#include "typedef.h"
#include "window.h"
#include "win32api.h"
#include <winreg.h>
#include <shellapi.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <tlhelp32.h>
#pragma comment(lib, "Kernel32.lib")
#include "CExplorerBgToolRe.h"

/*
	IsProcessRunning()

	Verifica se il processo specificato e' in esecuzione.
	(forma parte della classe ma in realta' potrebbe essere benissimo stand-alone)

	Parametri:
		[in]  pcwzProcessName: nome dell'eseguibile (es. "notepad.exe"), puo' includere
                               il percorso ma confronta comunque sia solo il nome file

	Ritorna:
		[out] BOOL: TRUE se il processo e' attivo, FALSE altrimenti
*/
BOOL IsProcessRunning(const wchar_t* pcwzProcessName)
{
	BOOL bFound = FALSE;
    
	// ricava uno snapshot di tutti i processi in esecuzione
	HANDLE hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
	if(hSnapshot==INVALID_HANDLE_VALUE)
		return(FALSE);

	PROCESSENTRY32W pe32 = {0};
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	// scorre la lista dei processi
	if(!::Process32FirstW(hSnapshot,&pe32))
	{
		::CloseHandle(hSnapshot);
		return(FALSE);
	}

	// estrae solo il nome file dal percorso fornito (se presente)
	const wchar_t* pwzTargetName = pcwzProcessName;
	const wchar_t* pwzLastSlash = wcsrchr(pcwzProcessName,L'\\');
	const wchar_t* pwzLastBackslash = wcsrchr(pcwzProcessName,L'/');
	if(pwzLastSlash!=NULL || pwzLastBackslash!=NULL)
		pwzTargetName = (pwzLastSlash > pwzLastBackslash) ? pwzLastSlash + 1 : pwzLastBackslash + 1;

	do {
		// confronta nome processo/nome eseguibile
		if(_wcsicmp(pe32.szExeFile,pwzTargetName)==0)
		{
			bFound = TRUE;
			break;
		}
	} while(::Process32NextW(hSnapshot,&pe32));

	::CloseHandle(hSnapshot);
	return(bFound);
}

/*
	GetDllPathByCLSID()

	Cerca nel registro il CLSID in formato stringa e restituisce, se rtovato, il percorso completo della 
	DLL registrata sotto la chiave InprocServer32.
	(forma parte della classe ma in realta' potrebbe essere benissimo stand-alone)

	Parametri:
		[in]  pcwzCLSID      : stringa del CLSID (es. L"{ED15A97D-FE3E-4CDE-98FF-BC46B02896B0}")
		[out] wzDllPathBuffer: buffer che ricevera' il percorso completo della DLL
		[in]  dwBufferSize   : dimensione del buffer espressa in caratteri (wchar_t)

	Ritorna:
		[out] BOOL: TRUE se trova il CLSID ed estrae il percorso, FALSE altrimenti
*/
BOOL GetDllPathByCLSID(const wchar_t* pcwzCLSID,wchar_t* wzDllPathBuffer,DWORD dwBufferSize)
{
	memset(wzDllPathBuffer,'\0',dwBufferSize);

	// compone il percorso completo della chiave InprocServer32
	wchar_t wzInprocSubKey[MAX_PATH+1] = {0};
	swprintf_s(wzInprocSubKey,MAX_PATH,L"CLSID\\%s\\InprocServer32",pcwzCLSID);

	// apre la chiave
	HKEY hInprocKey = NULL;
	LONG lResult = ::RegOpenKeyExW(HKEY_CLASSES_ROOT,wzInprocSubKey,0,KEY_READ,&hInprocKey);
	if(lResult!=ERROR_SUCCESS)
		return(FALSE); // Il CLSID non esiste o non ha un server InprocServer32 registrato

	// legge il valore predefinito
	// passando NULL come nome del valore, RegQueryValueExW() restituisce il valore di default della chiave aperta
	DWORD dwType = 0L;
	DWORD dwDataSizeInBytes = dwBufferSize * sizeof(wchar_t); // converte i caratteri in byte per l'API

	lResult = ::RegQueryValueExW(hInprocKey,NULL,NULL,&dwType,(LPBYTE)wzDllPathBuffer,&dwDataSizeInBytes);
	::RegCloseKey(hInprocKey);

	// verifica i dati letti e che sia tipo stringa
	if(lResult==ERROR_SUCCESS && dwType==REG_SZ)
		return(TRUE);

	memset(wzDllPathBuffer,'\0',dwBufferSize);
	return(FALSE);
}

// la costante CLSID per la DLL (ripresa dal progetto originale)
const wchar_t* CExplorerBgToolRe::CLSID_EXPLORERBGTOOL = L"{ED15A97D-FE3E-4CDE-98FF-BC46B02896B0}";

/*
	CExplorerBgToolRe()
*/
CExplorerBgToolRe::CExplorerBgToolRe()
{
	m_hDll = NULL;
	m_dwError = 0L;
}

/*
	~CExplorerBgToolRe()
*/
CExplorerBgToolRe::~CExplorerBgToolRe()
{
    UnloadDll();
}

/*
	Register()

	Registra la DLL nel sistema.

	Deve ricevere il pathname completo della DLL, restituisce S_OK o l'errore HRESULT.
*/
HRESULT CExplorerBgToolRe::Register(const wchar_t* pcwzDllPath)
{
	// definisce il tipo della funzione DllRegisterServer()
	typedef HRESULT (WINAPI *DllRegisterServerFunc)(void);

	// carica la DLL ed ottiene la funzione
	FARPROC proc = LoadFunction(pcwzDllPath,"DllRegisterServer");
	if(proc==NULL)
		return(E_FAIL);

	// casta il puntatore al tipo corretto
	DllRegisterServerFunc pRegister = (DllRegisterServerFunc)proc;

	// chiama la funzione di registrazione
	HRESULT hr = pRegister();

	// se la registrazione fallisce, rilascia la DLL
	if(FAILED(hr))
		UnloadDll();

	return(hr);
}

/*
	Unregister()
	Annulla la registrazione della DLL nel sistema.

	Deve ricevere il pathname completo della DLL, restituisce S_OK o l'errore HRESULT.
*/
HRESULT CExplorerBgToolRe::Unregister(const wchar_t* pcwzDllPath)
{
	// definisce il tipo della funzione DllUnregisterServer()
	typedef HRESULT (WINAPI *DllUnregisterServerFunc)(void);

	// carica la DLL ed ottiene la funzione
	FARPROC proc = LoadFunction(pcwzDllPath,"DllUnregisterServer");
	if(proc==NULL)
		return(E_FAIL);

	// cast del puntatore al tipo corretto
	DllUnregisterServerFunc pUnregister = (DllUnregisterServerFunc)proc;

	// chiama la funzione di deregistrazione
	HRESULT hr = pUnregister();

	// se la deregistrazione fallisce, rilascia la DLL
	if(FAILED(hr))
		UnloadDll();

	return(hr);
}

/*
    Unregister()

    Annulla la registrazione della DLL nel sistema senza richiedere il percorso.
    Ricava automaticamente il pathname della DLL interrogando il registro tramite il CLSID gia' noto della DLL.

    Ritorna S_OK, l'errore HRESULT o un codice di errore di Windows.
*/
HRESULT CExplorerBgToolRe::Unregister(void)
{
	wchar_t wzDllPath[_MAX_PATH+1] = {0};

	// interroga il registro usando il CLSID gia' noto
	// se la GetDllPathByCLSID() fallisce, significa che la DLL probabilmente non e' registrata
	// nel sistema o la chiave InprocServer32 e' stata cancellata, in ogni caso NON e' registrata
	if(!GetDllPathByCLSID(CLSID_EXPLORERBGTOOL,wzDllPath,_MAX_PATH))
		//return(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND));
		return(S_OK);

#if 0
	// il file della DLL esiste realmente o e' un fantasma?
	if(::GetFileAttributesW(wzDllPath)==INVALID_FILE_ATTRIBUTES)
		return(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
#else
	// controlla se il file esiste veramente su disco
	DWORD dwAttrib = ::GetFileAttributesW(wzDllPath);
	if(dwAttrib==INVALID_FILE_ATTRIBUTES) // && GetLastError()==ERROR_FILE_NOT_FOUND)
	{
		// file inesistente, pulisce il registro manualmente per evitare di lasciare la shell orfana
		wchar_t wzSubKey[MAX_PATH+1] = {0};

		// il percorso della sottochiave: CLSID\{...}\InprocServer32
		wsprintfW(wzSubKey,L"CLSID\\%s\\InprocServer32",CLSID_EXPLORERBGTOOL);
		::RegDeleteKeyW(HKEY_CLASSES_ROOT,wzSubKey);

		// cancella la chiave principale: CLSID\{...}
		wsprintfW(wzSubKey,L"CLSID\\%s",CLSID_EXPLORERBGTOOL);
		LSTATUS lRes = ::RegDeleteKeyW(HKEY_CLASSES_ROOT,wzSubKey);
		if(lRes==ERROR_SUCCESS)
			return(S_OK);
		else
			return(HRESULT_FROM_WIN32(lRes));
	}
#endif

	// il file della DLL esiste, lo passa al membro che si occupera'
	// di chiamare LoadLibrary() ed eseguire la DllUnregisterServer()
	return(Unregister(wzDllPath));
}

/*
	IsRegistered()

	Verifica se la DLL e' registrata, controllando il pathname specificato ed il CLSID gia' noto.
	Dal punto di vista teorico il passaggio ed il controllo del nome della DLL sono superflui.
	Ma dal punto di vista pratico, servono ad evitare "registrazioni fantasma", dato che se venisse
	controllato solo il CSLID, eventuali spostamenti del file o nuove versioni con nome differente,
	farebbero fallire i tentativi del sistema di caricare la DLL.
	In altre parole, il controllo NON e' per sapere se la DLL e' registrata o meno in assoluto, ma
	per verificare se il sistema e' configurato per caricare proprio la DLL specificata in input.

	Restituisce True se registrata, False altrimenti o Undef se registrata ma con nome file/percorso differente.
*/
TERN CExplorerBgToolRe::IsRegistered(const wchar_t* pcwzDllPath)
{
	wchar_t wzRegPath[_MAX_PATH+1] = {0};
	swprintf_s(wzRegPath,_MAX_PATH,L"CLSID\\%s\\InprocServer32",CLSID_EXPLORERBGTOOL);

	// verifica che esista la chiave, se no significa che la DLL non e' registrata
	HKEY hKey = NULL;
	LONG lResult = ::RegOpenKeyExW(HKEY_CLASSES_ROOT,wzRegPath,0,KEY_READ,&hKey);
	if(lResult!=ERROR_SUCCESS)
		return(False);

	// ricava il path completo della DLL associata alla chiave, controllando che il valore
	// non sia corrotto/mancante
	wchar_t wzRegisteredDllPath[_MAX_PATH+1] = {0};
	DWORD dwDataSize = sizeof(wzRegisteredDllPath);
	DWORD dwType = 0L;
	lResult = ::RegQueryValueExW(hKey,NULL,NULL,&dwType,(LPBYTE)wzRegisteredDllPath,&dwDataSize);
	::RegCloseKey(hKey);
	if(lResult!=ERROR_SUCCESS || dwType!=REG_SZ)
		return(False);

	// verifica se il percorso registrato e' lo stesso di quello specificato in input
	if(_wcsicmp(pcwzDllPath,wzRegisteredDllPath)==0)
		return(True);

	// se il percorso attualmente registrato per la DLL e' diverso da quello ricevuto 
	// in input, controllare l'esistenza del file
	if(::GetFileAttributesW(wzRegisteredDllPath)!=INVALID_FILE_ATTRIBUTES)
	{
		// il file esiste, ossia la DLL e' registrata con altro nome/percorso rispetto a quello di input
		return(Undef);
	}

	// se i percorsi non coincidono e la DLL registrata non esiste piu', allora e' una registrazione 
	// "fantasma" (orfana), ritorna False per far sapere che c'e' bisogno di una nuova registrazione
	return(False);
}

/*
	IsRegistered()

	Verifica se la DLL e' registrata, ricavando il nome completo di pathname del file (DLL) associato al CLSID.

	Restituisce TRUE se registrata e se il file relativo (DLL) esiste realmente, FALSE se non registrata o se il
	file (DLL) associato e' un fantasma.
*/
BOOL CExplorerBgToolRe::IsRegistered(wchar_t* pwzDllPath,size_t nSize) 
{
	wchar_t wzRegisteredPath[_MAX_PATH+1] = {0};
	if(!GetDllPathByCLSID(CLSID_EXPLORERBGTOOL,wzRegisteredPath,MAX_PATH))
		return(FALSE);

	wcscpy_s(pwzDllPath,nSize,wzRegisteredPath);

	// il file della DLL esiste realmente o e' un fantasma?
	BOOL bExist = ::GetFileAttributesW(wzRegisteredPath)!=INVALID_FILE_ATTRIBUTES;

	return(bExist);
}

/*
	IsAdmin()
	
	Verifica se il processo ha privilegi di amministratore.
	Usa OpenProcessToken() e GetTokenInformation() con TokenElevation, il metodo piu'
	affidabile a partire da Windows Vista.

	Restituisce TRUE se riesce, FALSE altrimenti. Notare che se fallisce le operazioni 
	di registrazione sulla DLL non potranno essere eseguite.
*/
BOOL CExplorerBgToolRe::IsAdmin(void)
{
	HANDLE hToken = NULL;
	BOOL bIsElevated = FALSE;

	// apre il token del processo corrente
	if(!::OpenProcessToken(::GetCurrentProcess(),TOKEN_QUERY,&hToken))
		return(FALSE);

	// ottiene le informazioni di elevazione
	TOKEN_ELEVATION elevation = {0};
	DWORD dwSize = sizeof(TOKEN_ELEVATION);
	if(::GetTokenInformation(hToken,TokenElevation,&elevation,dwSize,&dwSize))
		bIsElevated = elevation.TokenIsElevated;

	::CloseHandle(hToken);

	return(bIsElevated);
}

/*
	RestartExplorer()

	Riavvia l'Explorer (explorer.exe).
	Necessario per rendere effettivo il cambio prodotto con la registrazione/disattivazione della DLL.
	(da linea di comando: taskkill /f /im explorer.exe & start explorer.exe)

	Sui vecchi sistemi Windows era sufficente una sequenza come questa:

		// uccide explorer.exe
		system("taskkill /f /im explorer.exe");

		// aspetta che il sistema si stabilizzi
		Sleep(2000);

		// riavvia explorer.exe
		system("start explorer.exe");

	ma sulle versioni moderne (Windows 10/11) ci sono spesso piu' istanze di explorer.exe (taskbar + finestre di
	cartelle separate o processi figli). Se l'istanza che viene uccisa NON e' quella che tiene la DLL mappata in
	memoria, il file della DLL resta bloccato.
	Oltre al fatto che TerminateProcess() e' asincrona, per cui l'istanza che e' stata uccisa potrebbe non essere
	completamente morta quando si passa a manipolare il file della DLL.
	Bisogna quindi terminare tutte le istanze di Explorer, aspettare che siano effettivamente terminate (polling 
	dello snapshot) e solo alla fine manipolare il file della DLL e rilanciare l'explorer.exe.
*/
BOOL CExplorerBgToolRe::RestartExplorer(void)
{
	// uccide TUTTE le istanze e aspetta che spariscano
	// il ritardo e la sincronizzazione sono vitali, la Shell di Windows non e' istantanea e se
	// si rilancia explorer.exe troppo in fretta, Windows aprira' una finestra di cartella invece
	// di riavviare la taskbar
	const int MAX_RETRIES = 30; // 3 secondi totali di polling
	BOOL bAnyKilled = FALSE;

	for(int i=0; i < MAX_RETRIES; ++i)
	{
		HANDLE hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
		if(hSnapshot==INVALID_HANDLE_VALUE)
			return(FALSE);

		PROCESSENTRY32W pe = {0};
		pe.dwSize = sizeof(PROCESSENTRY32W);
		BOOL bFound = FALSE;

		if(::Process32FirstW(hSnapshot,&pe))
		{
			do {
				if(_wcsicmp(pe.szExeFile,L"explorer.exe")==0)
				{
					bFound = TRUE;

					// SYNCHRONIZE serve per WaitForSingleObject
					HANDLE hProcess = ::OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE,FALSE,pe.th32ProcessID);
					if(hProcess!=NULL)
					{
						::TerminateProcess(hProcess,0);
						::WaitForSingleObject(hProcess,2000); // aspetta che muoia davvero
						::CloseHandle(hProcess);
						bAnyKilled = TRUE;
					}
				}
			} while(::Process32NextW(hSnapshot,&pe));
		}

		::CloseHandle(hSnapshot);

		// nessun explorer.exe in esecuzione, puo' procedere
		if(!bFound)
			break;

		// se ne ha ucciso almeno uno, aspetta e rifa' lo snapshot perche'
		// Windows potrebbe respawnare o ci potrebbero essere altre istanze
		::Sleep(100);
	}

	// stabilizzazione extra
	::Sleep(500);

	// rilancia explorer.exe usando il percorso assoluto
	wchar_t szPath[MAX_PATH+1] = {0};
	if(!::GetWindowsDirectoryW(szPath,MAX_PATH))
		return(FALSE);
	wcscat_s(szPath,MAX_PATH,L"\\explorer.exe");

	// lancia il processo
	HINSTANCE hInst = ::ShellExecuteW(NULL,L"open",szPath,NULL,NULL,SW_SHOWNORMAL);

	// cast a intptr_t invece che int per sicurezza sui puntatori
	return((intptr_t)hInst > 32);
}

/*
	LoadFunction()

	Carica la DLL e restituisce il puntatore alla funzione specificata.
*/
FARPROC CExplorerBgToolRe::LoadFunction(const wchar_t* pcwzDllPath,const char* pProcName)
{
	UnloadDll();

	// verifica se il file esiste
	if(::GetFileAttributesW(pcwzDllPath)==INVALID_FILE_ATTRIBUTES)
	{
		m_dwError = ERROR_FILE_NOT_FOUND;
		return(NULL);
	}

	// prova a caricare la DLL
	::SetLastError(0);
	m_hDll = ::LoadLibraryW(pcwzDllPath);
	if(m_hDll==NULL)
	{
		m_dwError = ::GetLastError();
		return(NULL);
	}

	// cerca la funzione
	FARPROC pProcAddr = ::GetProcAddress(m_hDll,pProcName);
	if(pProcAddr==NULL)
	{
		m_dwError = ::GetLastError();
		UnloadDll();
		return(NULL);
	}

	return(pProcAddr);
}

/*
	UnloadDll()

	Rilascia (scarica) la DLL.
*/
void CExplorerBgToolRe::UnloadDll(void)
{
	if(m_hDll!=NULL)
	{
		::FreeLibrary(m_hDll);
		m_hDll = NULL;
		m_dwError = 0L;
	}
}
