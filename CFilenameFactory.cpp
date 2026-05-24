/*$
	CFilenameFactory.cpp
	Classe base per la gestione dei nomi files (SDK/MFC).
	Luca Piergentili, 02/09/03
	lpiergentili@yahoo.com
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include "win32api.h"
#include "CFilenameFactory.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	Abbreviate()
	
	Accorcia il pathname/nomefile a fini estetici.
	Solo per path con nomi file MS-DOS, non per path Unix o per URLs.

	Il secondo parametro stabilisce la lunghezza massima dell'output: se viene passato come valore positivo, accorcia
	alla fine (salvando o meno l'estensione del nome del file a seconda del flag):
	
		C:\Program Files (x86)\MS Products\Development\VS2022 E[...].com
		C:\Program Files (x86)\MS Products\Development\VS2022 Enter[...]

	se invece viene passato negativo, nel caso in cui sia presente un pathname, inserisce l'abbreviazione nell'ultima
	parte del path prima del nome del file (salvando o meno l'estensione del nome del file a seconda del flag):

		C:\Program Files (x86)\[...]\WhyMicrosoftSucksBallsBigTime.com

	se il nome de file gia' eccede la lunghezza massima da solo, allora elimina il path e accorcia direttamente il nome

	stringhe per esempi pratici:

	char str[1024] = {"C:\\Program Files (x86)\\MS Products\\Development\\VS2022 Enterprise Edition\\Microsoft Visual Studio\\Installer\\PrecheckTools\\WhyMicrosoftSucksBallsBigTime.com"};
	char str1[1024] = {"C:\\Program Files (x86)\\MS Products\\Development\\VS2022 Enterprise Edition\\Microsoft Visual Studio\\Installer\\PrecheckTools\\WhyMicrosoftSucksBallsBigTimeIDontKnowWhyButICanTellYouThatIsVeryAnnoingYouKnowWhoYouAre.com"};
	char str2[1024] = {"WhyMicrosoftSucksBallsBigTimeIDontKnowWhyButICanTellYouThatIsVeryAnnoingYouKnowWhoYouAre.com"};
*/
LPCSTR CFilenameFactory::Abbreviate(	LPCSTR	lpcszFilename,
										int		nMaxLength,		/* = ABBREVIATE_MAX_FNAME */
										BOOL	bSaveExt,		/* = FALSE */
										BOOL	bStripPath,		/* = TRUE */
										LPCSTR	pDefaultString,	/* = NULL */
										LPSTR	pFilename,		/* = NULL */
										UINT	nFilenameSize	/* = 0 */
									)
{
	ASSERTEXPR(lpcszFilename);
	if(!lpcszFilename)
		return(pFilename);

	// solo per path con nomi file MS-DOS, non per path Unix o per URLs
	if(strchr(lpcszFilename,'/'))
		return(pFilename);

	char* p;
	char szFileName[_MAX_FILEPATH+1] = {0};
	char szExt[_MAX_FILEPATH+1] = {0};

	// se deve eliminare il path dal nome file
	if(bStripPath)
	{
		p = (char*)strrchr(lpcszFilename,'\\');
		if(p)
			p++;
		if(!p)
			p = (char*)lpcszFilename;
		strcpyn(szFileName,p,sizeof(szFileName));
	}
	else
		strcpyn(szFileName,lpcszFilename,sizeof(szFileName));

	// imposta la stringa per l'abbreviazione
	if(!pDefaultString)
		pDefaultString = ABBREVIATE_DEFAULT_STRING;

	// ricava (e fa fuori dal nome) l'estensione del file
	if(bSaveExt)
	{
		p = strrchr(szFileName,'.');
		if(p)
		{
			strcpyn(szExt,p,sizeof(szExt));
			*p = '\0';
		}
	}

	// se == 0 non fa nulla, se il valore e' > 0 accorcia alla fine e se e' < 0 accorcia 
	// tra gli ultimi due backslash se presenti (un subpath) o alla fine in caso contrario
	if(nMaxLength==0)
	{
		; // la unica utilita' e' per eliminare il path, vedi sopra
	}
	else if(nMaxLength > 0)
	{
onlyfile:
		// se la lunghezza totale (nome+ext) sfora il limite
		if((int)(strlen(szFileName) + strlen(szExt)) > nMaxLength)
		{
			int n = strlen(pDefaultString);
			
			// copia la stringa per l'abbreviazione troncando il nome file alla lunghezza desiderata
			memcpy(szFileName+(nMaxLength-(n+strlen(szExt))),pDefaultString,n);
			szFileName[nMaxLength-strlen(szExt)] = '\0';
			n = strlen(szFileName);
			if(*szExt)
				snprintf(szFileName+n,sizeof(szFileName)-n,"%s",szExt);
		}
		else // nessuno sforamento, riattacca l'estensione
		{
			if(*szExt)
				strcatn(szFileName,szExt,sizeof(szFileName));
		}
	}
	else if(nMaxLength < 0)
	{
		// rimette in positivo per poter controllare la lunghezza max
		nMaxLength *= -1;

		if((int)(strlen(szFileName) + strlen(szExt)) > nMaxLength)
		{
			// divide il nome file in path e file
			char szName[_MAX_FILEPATH+1] = {0};
			char szPath[_MAX_FILEPATH+1] = {0};
			strcpyn(szPath,szFileName,sizeof(szPath));
			char* p = strrchr(szPath,'\\');
			if(p)
			{
				strcpyn(szName,p+1,sizeof(szName));
				*p = '\0';
			}
			else // il nome file non contiene nessun path
			{
				memset(szPath,'\0',sizeof(szPath));
				strcpyn(szName,szFileName,sizeof(szName));
			}

			// inizia ad eliminare i subpath a partire da destra
			// controlla che il nome da solo non ecceda la lunghezza massima, esclude del tutto l'eventuale
			// path e salta al codice per troncare direttamente il nome file
			// se gia' il nome da solo sfora, elimina il path perche' altrimenti l'accorciamento con il path 
			// incluso nel nome file eliminerebbe del tutto il nome del file
			if((int)(strlen(szName)+strlen(pDefaultString)+strlen(szExt)+2) <= nMaxLength)
			{
	looop:		// elimina fino a che esistono almeno due backslash
				int bs = strcount(szPath,"\\");
				if(bs >= 2)
				{
					p = strrchr(szPath,'\\');
					if(p)
					{
						*p = '\0';
						if((int)(strlen(szPath)+strlen(pDefaultString)+strlen(szName)+strlen(szExt)) > nMaxLength)
							goto looop;
					}
				}

				// ricompone il nome file accorciato
				snprintf(szFileName,sizeof(szFileName),"%s\\%s\\%s%s",szPath,pDefaultString,szName,szExt);
			}
			else // gia' il solo nome file sfora, salta diretto ad accorciare, non considera il path
			{
				strcpyn(szFileName,szName,sizeof(szFileName));
				goto onlyfile;
			}
		}
		else // nessuno sforamento, riattacca l'estensione
		{
			if(*szExt)
				strcatn(szFileName,szExt,sizeof(szFileName));
		}
	}

	// se non viene passato un buffer di output per il nuovo nome file, usa e restituisce il membro della classe, il che non e' igenico...
	LPSTR pOutputBuffer = pFilename;
	UINT nOutputSize = nFilenameSize;
	if(!pOutputBuffer || nOutputSize==0)
	{
		pOutputBuffer = m_szFileName;
		nOutputSize = sizeof(m_szFileName);
	}

	strcpyn(pOutputBuffer,szFileName,nOutputSize);

	return(pOutputBuffer);
}

/*
	GetNext()

	Costruisce un nuovo nomefile, aggiungendo un progressivo numerico (a base 1) al nome gia' esistente.
	Incremento infinito.
	Funzionalmente uguale alla YetAnotherFileName() in win32api.c.
*/
LPCSTR CFilenameFactory::GetNext(LPCSTR lpcszFilename,LPCSTR lpcszPathname/* = NULL*/,LPSTR pFilename /* = NULL*/,UINT nFilenameSize/* = 0*/)
{
	ASSERTEXPR(lpcszFilename);
	if(!lpcszFilename)
		return(pFilename);

	// se non viene passato un buffer di output per il nuovo nome file, usa e restituisce il membro della classe, il che non e' igenico...
	LPSTR pOutputBuffer = pFilename;
	UINT nOutputSize = nFilenameSize;
	if(!pOutputBuffer || nOutputSize==0)
	{
		pOutputBuffer = m_szFileName;
		nOutputSize = sizeof(m_szFileName);
	}

	// unisce nome file e pathname
	if(!lpcszPathname || !*lpcszPathname || strcmp(lpcszPathname,".")==0)
		snprintf(pOutputBuffer,nOutputSize,"%s",lpcszFilename);
	else
		snprintf(pOutputBuffer,nOutputSize,"%s%s%s",lpcszPathname,lpcszPathname[strlen(lpcszPathname)-1]=='\\' ? "" : "\\",lpcszFilename);
	
	// se non esiste, restituisce il nome cosi' come lo ha ricevuto (unito al pathname)
	if(FileExists(pOutputBuffer))
	{
		int n;
		int i = 0;
		char* p;
		char szFileName[_MAX_FILEPATH+1] = {0};
		char szExt[_MAX_FILEPATH+1] = {""};
	
		// costruisce il nuovo nome aggiungendo un progressivo numerico: "name.ext" -> "name (n).ext"
		do
		{
			strcpyn(szFileName,pOutputBuffer,sizeof(szFileName));
			p = strrchr(szFileName,'.');
			if(p)
			{
				strcpyn(szExt,p,sizeof(szExt));
				*p = '\0';
			}
			n = strlen(szFileName);
			snprintf(szFileName+n,sizeof(szFileName)-n," (%d)%s",++i,szExt);
		} while(FileExists(szFileName));

		strcpyn(pOutputBuffer,szFileName,nOutputSize);
	}

	return(pOutputBuffer);
}

/*
	GetNextWithinRange()

	Costruisce un nuovo nomefile aggiungendo un progressivo numerico (a base 1) al nome gia' esistente, dentro 
	l'intervallo numerico da 1 a <n>.
	Effettua una rotazione tra i nomi file compresi nell'intervallo, all'inizio con il progressivo e poi, una
	volta esauriti i progressivi disponibili, in base alla data/ora.
	Il precursore (il nome file senza progressivo) non viene considerato durante la rotazione.
*/
BOOL CFilenameFactory::GetNextWithinRange(LPCSTR lpcszFilename,UINT nMaxProg,LPSTR pFilename,UINT nFilenameSize)
{
	ASSERTEXPR(lpcszFilename && pFilename && nMaxProg > 0);
	if(!lpcszFilename || !pFilename || nMaxProg <= 0)
		return(FALSE);

	// se il file non esiste, lo considera come il precursore della serie numerata
	if(!FileExists(lpcszFilename))
	{
		strcpyn(pFilename,lpcszFilename,nFilenameSize);
		return(TRUE);
	}

    char dirPart[_MAX_FILEPATH+1] = {0};
    char namePart[_MAX_FILEPATH+1] = {0};
    char extPart[_MAX_FILEPATH+1] = {0};

    // cerca l'ultimo backslash e l'ultimo punto per estrarre il nome file dal (eventuale) path
    const char* lastSlash = strrchr(lpcszFilename,'\\');
    const char* lastDot = strrchr(lpcszFilename,'.');

    // estrazione path
    const char* pNameStart = lpcszFilename; // se non c'e' backslash
    if(lastSlash)
	{
        // copia tutto fino al backslash incluso
        size_t dirLen = (lastSlash - lpcszFilename) + 1;
        strncpy_s(dirPart,sizeof(dirPart),lpcszFilename,dirLen);
        pNameStart = lastSlash + 1;
    }

    // estrae estensione e nome
    if(lastDot && lastDot > lastSlash)
	{
        // c'e' un'estensione ed e' dopo l'ultimo backslash (quindi appartiene al file)
        strcpyn(extPart,lastDot,sizeof(extPart));
        
        // il nome e' tra l'inizio del nome ed il punto
        size_t nameLen = lastDot - pNameStart;
        strncpy_s(namePart,sizeof(namePart),pNameStart,nameLen);
    }
	else
	{
        // nessuna estensione valida
        strcpyn(namePart,pNameStart,sizeof(namePart));
    }

    // calcola il padding (0 iniziali) in base al numero max per il prog
	char szNum[32] = {0};
	snprintf(szNum,sizeof(szNum),"%ld",nMaxProg);
	UINT nPaddingWidth = strlen(szNum);

    // verifica esistenza 0..n
    char candidatePath[_MAX_FILEPATH+1] = {0};
    for(int i = 1; i <= (int)nMaxProg; i++)
	{
        BuildCandidateName(candidatePath,sizeof(candidatePath),dirPart,namePart,extPart,i/*,nPaddingWidth*/);

        // controllo esistenza
        DWORD dwAttrib = ::GetFileAttributes(candidatePath);

		if(dwAttrib==INVALID_FILE_ATTRIBUTES && ::GetLastError()==ERROR_FILE_NOT_FOUND)
		{
            // trovato un buco! il file non esiste, lo usa
            strcpyn(pFilename,candidatePath,nFilenameSize);
            return(TRUE);
        }
    }

    // rotazione: ricerca del piu' vecchio
    // se arriva qui, tutti i file da 0 a n esistono, deve quindi cercare il piu' vecchio per data/ora
    
    int oldestIndex = -1;
    FILETIME oldestTime = {0};
    // inizializza oldestTime al valore massimo possibile (o usa il primo file come riferimento)
    oldestTime.dwHighDateTime = 0xFFFFFFFF;
    oldestTime.dwLowDateTime = 0xFFFFFFFF;

    for(int i = 1; i <= (int)nMaxProg; i++)
	{
        BuildCandidateName(candidatePath,sizeof(candidatePath), dirPart, namePart, extPart, i/*,nPaddingWidth*/);

        FILETIME ft = {0};
        if(GetFileLastTime(candidatePath,&ft))
		{
            // restituisce -1 se ft < oldestTime (ft e' piu' vecchio)
            if(CompareFileTime(&ft,&oldestTime) < 0)
			{
                oldestTime = ft;
                oldestIndex = i;
            }
        }
    }

    if(oldestIndex!=-1)
	{
        // ha trovato il vincitore (il perdente, in realta', perche' verra' sovrascritto)
        BuildCandidateName(pFilename,nFilenameSize,dirPart,namePart,extPart,oldestIndex/*,nPaddingWidth*/);
        return(TRUE);
    }

    // fallback in caso di errori strani (es. file lockati che non si possono leggere)
    // restituisce il primo della serie per sicurezza
    BuildCandidateName(pFilename,nFilenameSize,dirPart,namePart,extPart,1/*,nPaddingWidth*/);
    return(TRUE);
}

/*
	BuildCandidateName()

	Helper per costruire il nome file, basicamente effettua il merge delle varie componenti.
*/
void CFilenameFactory::BuildCandidateName(	LPSTR	pFilename,
											UINT	nFilenameSize,
											LPCSTR	lpcszDir, 
											LPCSTR	lpcszName,
											LPCSTR	lpcszExt, 
											UINT	nProg/*, 
											UINT	nPaddingWidth*/
											)
{
	ASSERTEXPR(pFilename && nFilenameSize > 0);
	if(!pFilename || nFilenameSize <= 0)
		return;

    // formato: %s (path) %s (nome) %0*d (numero con zeri) %s (ext)
    // es.: C:\Logs\ + mylog + 045 + .txt
	//_snprintf_s(dest,destSize,_TRUNCATE,"%s%s%0*d%s",dirPart,namePart,nPaddingWidth,number,extPart);

    // formato: path + nome + spazio + (n) + ext
    snprintf(pFilename,nFilenameSize,"%s%s (%d)%s",lpcszDir,lpcszName,nProg,lpcszExt);
}
