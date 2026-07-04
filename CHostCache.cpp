/*$
	CHostCache.cpp
	Classe per la cache dei nomi host.
	Luca Piergentili, Settembre '25
*/
#include "pragma.h"
#include <stdio.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include "ipaddress.h"
#include "CWinsock.h"
#include "CHostCache.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CHostCache()
*/
CHostCache::CHostCache()
{
	memset(m_chacheArray,'\0',sizeof(m_chacheArray));
}

/*
	~CHostCache()
*/
CHostCache::~CHostCache()
{
#ifdef _DEBUG
	for(int i=0; i < CACHE_SIZE; i++)
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"counter: %d, host: %s, ip: %s, wsaerror: %d\n",m_chacheArray[i].counter,m_chacheArray[i].hostname,m_chacheArray[i].ipaddress,m_chacheArray[i].wsaerror));
#endif
}

/*
	ResolveHostWithCache()

	Verifica la cache per il nome host specificato, lo risolve con gethostbyname 
	se non lo trova aggiorna la cache (con logica LFU).
	
	Restituisce 0 se l'host e' stato risolto (dalla cache o dalla rete), o il codice 
	d'errore Winsock altrimenti.
*/
int CHostCache::ResolveHost(LPCSTR lpcszHostname,LPSTR lpszIPAddress,UINT nIPAddress)
{
	int nWSAError = 0;
	int nCacheIndex;
    
    // cerca il nome host nella cache
    nCacheIndex = FindCacheEntry(lpcszHostname);

	// CACHE HIT: host trovato
	if(nCacheIndex!=-1)
    {
		// copia l'ip relativo al nome host nel buffer del chiamante
		strcpyn(lpszIPAddress,m_chacheArray[nCacheIndex].ipaddress,nIPAddress);

		// incrementa il contatore d'uso (LFU)
		m_chacheArray[nCacheIndex].counter++;
        
		//TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"Host found (%d): %s - %s\n",m_chacheArray[nCacheIndex].counter,m_chacheArray[nCacheIndex].hostname,m_chacheArray[nCacheIndex].ipaddress));
    }
	// CACHE MISS: host non presente nella cache
	else
	{
		struct hostent *hostEntry;

		// risolve il nome dell'host (IPv4)
		hostEntry = m_Socket.gethostbyname(lpcszHostname);
		if(hostEntry!=NULL)
		{
			struct in_addr addr;
			memcpy(&addr,hostEntry->h_addr_list[0],hostEntry->h_length);		// ip in forma binaria
			strcpyn(lpszIPAddress,m_Socket.inet_ntoa(addr),nIPAddress);			// ip in forma di stringa

			//TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"Host NOT found, resolved: %s - %s\n",lpcszHostname,lpszIPAddress));
		}
		else
		{
			memset(lpszIPAddress,'\0',nIPAddress);								// azzera il buffer del chiamante
			nWSAError = m_Socket.WSAGetLastError();								// ricava l'errore Winsck

			//TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"Host NOT found, Winsock error %d: %s\n",nWSAError,lpcszHostname));
		}

		// registra il risultato (positivo o errore) nella cache
		// cerca nella cache un elemento libero, ossia con il contatore a 0, 
		// o se tutti occupati, quello con il contatore piu' basso
		nCacheIndex = FindLFUIndex();
		if(nCacheIndex!=-1)
		{
			// inserisce il nome host
			strcpyn(m_chacheArray[nCacheIndex].hostname,lpcszHostname,sizeof(m_chacheArray[nCacheIndex].hostname));
        
			// inserisce l'ip
			strcpyn(m_chacheArray[nCacheIndex].ipaddress,lpszIPAddress,sizeof(m_chacheArray[nCacheIndex].ipaddress));

			// inizializza il contatore
			m_chacheArray[nCacheIndex].counter = 1; 

			// errore Winsock
			if(nWSAError!=0)
				m_chacheArray[nCacheIndex].wsaerror = nWSAError;
		}
	}

    return(nWSAError);
}

/*
	FindCacheEntry()

	Cerca l'host nella cache, restituendo l'indice relativo o -1 se non trovato.
*/
int CHostCache::FindCacheEntry(const char* lpcszHostname)
{
	for(int i=0; i < CACHE_SIZE; i++)
	{
		// controlla non solo il nome host ma che non sia marcato come libero
		if(m_chacheArray[i].counter!=CACHE_ENTRY_FREE && stricmp(m_chacheArray[i].hostname,lpcszHostname)==0)
			return(i);
	}

	return(-1);
}

/*
	FindLFUIndex()

	Cerca nella cache l'elemento con il contatore di uso piu' basso e ne restituisce l'indice.
	Se trova un elemento libero lo restitisce per primo.
*/
int CHostCache::FindLFUIndex(void)
{
    unsigned int minCount = 0xFFFFFFFF; // il massimo valore possibile
    int lfuIndex = -1;

    for(int i=0; i < CACHE_SIZE; i++)
    {
        // priorita' 1: se l'elemento e' libero, e' la scelta migliore
        if(m_chacheArray[i].counter==CACHE_ENTRY_FREE)
            return(i);
        
        // priorita' 2: se il contatore e' inferiore al minimo attuale, lo 
		// aggiorna con l'indice dell'elemento per restituirlo sotto
        if(m_chacheArray[i].counter < minCount)
        {
            minCount = m_chacheArray[i].counter;
            lfuIndex = i;
        }
    }
    
    // restituisce l'indice con il contatore d'uso piu' basso (o -1 se qualcosa va storto)
    return(lfuIndex);
}
