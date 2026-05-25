/*$
	CFifoLifo.cpp
	Classi per liste FIFO/LIFO.
	Implementa la logica FIFO/LIFO usando una lista di tipo CNodeList.
	Luca Piergentili, Dicembre '18

	Vedi le note in CFifoLifo.h
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#ifdef _WINDOWS
  #include "window.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "typedef.h"
#include "CNodeList.h"
#include "CFifoLifo.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	cLIFO()
*/
cLIFO::cLIFO()
{
	m_nCounter = 0;		// progressivo per identificare l'oggetto inserito nella lista, verra' usato per le ricerca
	m_nTotalSize = 0;	// limite elementi della lista: nel momento in cui la lista sta a 0, se il contatore supera il limite, resetta la lista
}

/*
	~cLIFO()
*/
cLIFO::~cLIFO()
{
}

/*
	Push()

	Inserisce l'elemento nella lista.
	Il chiamante deve passare il puntatore ai dati che devono essere immagazzinati.

	Restituisce TRUE per inserimento avvenuto, FALSE altrimenti.
*/
BOOL cLIFO::Push(void* ptr)
{
	// crea ed aggiunge un nodo alla lista
	LIFO* lifo = (LIFO*)m_LIFOList.Add();
	if(lifo)
	{
		m_nTotalSize++;					// totale dei nodi della lista
		m_nCounter++;					// stabilisce il progressivo da usare per immagazzinare (e poi ricercare) l'elemento
		lifo->counter = m_nCounter;		// salva il progressivo nel nodo
		lifo->ptr = ptr;				// salva i dati utente (l'oggetto) nel nodo
		return(TRUE);
	}
	return(FALSE);
}

/*
	Pop()

	Ricava il seguente elemento dalla lista (dall'ultimo ad essere stato inserito fino al primo).

	Restituisce il puntatore ai dati immagazzinati o NULL se non esistono piu' elementi
	nella lista (gia' sono stati estratti tutti).
*/
void* cLIFO::Pop(void)
{
	ITERATOR iter;
	void* ptr = NULL;

	// gli elementi poppati vengono marcati con -1 e rimangono fisicamente nella lista (agganciati al nodo): il 
	// nodo relativo non puo' essere eliminato, altrimenti il chiamante vedrebbe scomparire l'oggetto da sotto i 
	// piedi, quindi non puo' guidarsi solo in base a quanti elementi ha la lista, ma deve considerare anche il
	// contatore logico che aggiorna ad ogni push/pop
	if(m_nCounter > 0 && (iter = m_LIFOList.First())!=(ITERATOR)NULL)
	{
		// ricava il progressivo dell'elemento da estrarre
		// il contatori va aggiornato una volta trovato l'elemento, vedi sotto
		LIFO* pData;
		int nCounter = m_nCounter;

		// scorre la lista cercando il progressivo relativo
		do
		{
			pData = (LIFO*)iter->data;
			if(pData)
			{
				// ha trovato l'elemento, aggiorna il contatore, imposta il progressivo dell'elemento come
				// estratto e ricava e restituisce il puntatore ai dati (l'oggetto) SENZA eliminarlo
				if(pData->counter==nCounter)
				{
					m_nCounter--;
					pData->counter = -1;
					ptr = pData->ptr;
					return(ptr);
				}
			}

			iter = m_LIFOList.Next(iter);
				
		} while(iter!=(ITERATOR)NULL);
	}

	return(ptr);
}

/*
	Reset()

	Elimina gli elementi della lista.
	Solo chi utilizza la classe, non la classe stessa, puo' sapere quando e' sicuro azzerare la lista.
*/
void cLIFO::Reset(void)
{
	if(m_nCounter==0 && (m_nTotalSize > MAX_FIFOLIFO_ITEMS_AT_TIME))
	{
		m_LIFOList.DeleteAll();
		m_nTotalSize = 0;
	}
}

/*
	cFIFO()
*/
cFIFO::cFIFO()
{
	m_nCounter = 0;		// progressivo per identificare l'oggetto inserito nella lista, verra' usato per le ricerca
	m_nLastPop = 0;		// progressivo per l'indice dell'ultimo elemento estratto dalla lista con pop
	m_nTotalSize = 0;	// limite elementi della lista: nel momento in cui la lista sta a 0, se il contatore supera il limite, resetta la lista
}

/*
	~cFIFO()
*/
cFIFO::~cFIFO()
{
}

/*
	Push()

	Inserisce l'elemento nella lista.
	Il chiamante deve passare il puntatore ai dati che devono essere immagazzinati.

	Restituisce TRUE per inserimento avvenuto, FALSE altrimenti.
*/
BOOL cFIFO::Push(void* ptr)
{
	// crea ed aggiunge un nodo alla lista
	LIFO* lifo = (LIFO*)m_FIFOList.Add();
	if(lifo)
	{
		m_nTotalSize++;					// totale dei nodi della lista
		m_nCounter++;					// stabilisce il progressivo da usare per immagazzinare (e poi ricercare) l'elemento
		lifo->counter = m_nCounter;		// salva il progressivo nel nodo
		lifo->ptr = ptr;				// salva i dati utente (l'oggetto) nel nodo
		return(TRUE);
	}
	return(FALSE);
}

/*
	Pop()

	Ricava il seguente elemento dalla lista (dal primo ad essere stato inserito fino all'ultimo).

	Restituisce il puntatore ai dati immagazzinati o NULL se non esistono piu' elementi
	nella lista (gia' sono stati estratti tutti).
*/
void* cFIFO::Pop(void)
{
	ITERATOR iter;
	void* ptr = NULL;

	// gli elementi poppati vengono marcati con -1 e rimangono fisicamente nella lista (agganciati al nodo): il 
	// nodo relativo non puo' essere eliminato, altrimenti il chiamante vedrebbe scomparire l'oggetto da sotto i 
	// piedi, quindi non puo' guidarsi solo in base a quanti elementi ha la lista, ma deve considerare anche il
	// contatore logico che aggiorna ad ogni push/pop
	if(m_nCounter > 0 && (iter = m_FIFOList.First())!=(ITERATOR)NULL)
	{
		// ricava il progressivo dell'elemento da estrarre
		// il contatore va aggiornato una volta trovato l'elemento, vedi sotto
		LIFO* pData;
		int nCounter = m_nLastPop+1;

		// scorre la lista cercando il progressivo relativo
		do
		{
			pData = (LIFO*)iter->data;
			if(pData)
			{
				// ha trovato l'elemento, aggiorna il contatore, imposta il progressivo dell'elemento come
				// estratto e ricava e restituisce il puntatore ai dati (l'oggetto) SENZA eliminarlo
				if(pData->counter==nCounter)
				{
					m_nLastPop++;
					pData->counter = -1;
					ptr = pData->ptr;
					return(ptr);
				}
			}

			iter = m_FIFOList.Next(iter);
				
		} while(iter!=(ITERATOR)NULL);
	}

	return(ptr);
}

/*
	Reset()

	Elimina gli elementi della lista.
	Solo chi utilizza la classe, non la classe stessa, puo' sapere quando e' sicuro azzerare la lista.
*/
void cFIFO::Reset(void)
{
	if((m_nCounter==m_nLastPop && m_nCounter > 0) && (m_nTotalSize > MAX_FIFOLIFO_ITEMS_AT_TIME))
	{
		m_FIFOList.DeleteAll();
		m_nCounter = m_nLastPop = m_nTotalSize = 0;
	}
}