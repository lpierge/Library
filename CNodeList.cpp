/*$
	CNodeList.cpp
	Classe base per la gestione di una lista semplice (NON ordinata) con riutilizzo (automatico) degli elementi eliminati (CRT/SDK/MFC).
	Workaround per evitare i templates, centrata sulla riutilizzazione.
	Luca Piergentili, 05/01/98
	lpiergentili@yahoo.com

	Vedi le note in CNodeList.h
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

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
//#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CreateNode()

	Crea un nuovo nodo e lo inserisce nella lista
	(prima di creare un nuovo nodo controlla se ne esistono di disponibili).
	Deve ricevere il puntatore ai dati da associare al nodo.
*/
CNode* CNodeList::CreateNode(void* ptr)
{
	CNode* pNode;

	ASSERTEXPR(ptr);

	// controlla se esistono nodi inutilizzati
	if((pNode = FindFirstNode(UNUSED_NODE))!=(CNode*)NULL)
	{
		// riusa il nodo inutilizzato, associandogli i dati
		InitializeNode(pNode,USED_NODE,ptr);
	}
	else
	{
		// nessun nodo disponibile, ne crea uno nuovo
		pNode = (CNode*)new CNode();
		ASSERTEXPR(pNode);
		if(pNode!=(CNode*)NULL)
		{
			// inserisce un nuovo nodo associandogli i dati
			InitializeNode(pNode,UNUSED_NODE,ptr);
			InsertNode(pNode);
		}
	}
	
	// indice del nodo/totale dei nodi presenti
	if(pNode)
		pNode->index = m_nTot++;

	// rinumera i nodi occupati
	EnumerateNodes();

	return(pNode);
}

/*
	CreateNode()

	Crea un nuovo nodo e lo inserisce nella lista (prima di creare un nuovo nodo controlla se ne esistono
	di disponibili).
	I dati da associare al nodo vengono creati dinamicamente chiamando la virtuale Create() nel caso in cui
	debba creare un nuovo nodo, se invece riutilizza un nodo gia' esistente chiama la virtuale Initialize()
	per reinizializzare i dati.
*/
void* CNodeList::CreateNode(void)
{
	CNode* pNode;

	// controlla se esistono nodi inutilizzati
	pNode = FindFirstNode(REUSED_NODE);
	if(!pNode)
		pNode = FindFirstNode(UNUSED_NODE);
	
	if(pNode)
	{
		// riusa il nodo unitilizzato associandogli i dati gia' esistenti
		InitializeNode(pNode,USED_NODE,Initialize(pNode->data));
	}
	else
	{
		// nessun nodo disponibile, ne crea uno nuovo
		pNode = (CNode*)new CNode();
		ASSERTEXPR(pNode);
		if(pNode!=(CNode*)NULL)
		{
			// inserisce un nuovo nodo associandogli i dati gia' esistenti
			InitializeNode(pNode,UNUSED_NODE,Create());
			Initialize(pNode->data);
			InsertNode(pNode);
		}
	}
	
	// indice del nodo/totale dei nodi presenti
	if(pNode)
		pNode->index = m_nTot++;

	// rinumera i nodi occupati
	EnumerateNodes();

	return(pNode->data);
}

/*
	InitializeNode()

	Inizializza il nodo.
	Non inizializza il puntatore al nodo successivo dato che i nodi inutilizzati vengono 
	riciclati (il ptr al nodo successivo viene inizializzato solo quando si inserisce un
	nuovo nodo nella lista con InsertNode()).
*/
void CNodeList::InitializeNode(CNode* pNode,int status,void* ptr)
{
#ifdef _DEBUG
	memset(pNode->signature,'\0',SIGNATURE_LEN+1);
	strcpyn(pNode->signature,Signature(),SIGNATURE_LEN+1);
#endif
	pNode->index  = 0;
	pNode->status = status;
	pNode->data   = ptr;
}

/*
	InsertNode()

	Inserisce il nodo nella lista.
*/
void CNodeList::InsertNode(CNode* pNode)
{
	ASSERTEXPR(pNode);

	// marca il nodo come occupato
	pNode->status = USED_NODE;

	// imposta il puntatore al nodo successivo
	pNode->next = (CNode*)NULL;

	// inserisce il nodo nella lista
	if(m_pFirstNode==(CNode*)NULL)
		m_pFirstNode = m_pLastNode = pNode;
	else
		m_pLastNode = m_pLastNode->next = pNode;
}

/*
	CheckNode()

	Controlla la validita' del nodo.
*/
#ifdef _DEBUG
BOOL CNodeList::CheckNode(CNode* pNode)
{
	BOOL flag = FALSE;

	ASSERTEXPR(pNode);

	// controlla i puntatori e confronta la signature
	if(pNode)
		if(*pNode->signature)
			flag = memcmp(pNode->signature,Signature(),strlen(Signature()))==0;

	ASSERTEXPR(flag);

	return(flag);
}
#endif

/*
	CountNodes()

	Conta i nodi (occupati) presenti nella lista.
*/
int CNodeList::CountNodes(void)
{
	int tot = 0;
	CNode* pNode = m_pFirstNode;
#ifdef _DEBUG
	BOOL bIsValid = FALSE;
#endif

	// scorre la lista
	while(pNode!=(CNode*)NULL)
	{
#ifdef _DEBUG
		// controlla la signature
		bIsValid = CheckNode(pNode);
		ASSERTEXPR(bIsValid);
		if(!bIsValid)
			break;
#endif
		// controlla lo status ed incrementa il totale dei nodi occupati
		if(pNode->status==USED_NODE)
			tot++;

		// passa al nodo successivo
		pNode = pNode->next;
	}

	return(tot);
}

/*
	EnumerateNodes()

	Rinumera i nodi (occupati) presenti nella lista.
*/
void CNodeList::EnumerateNodes(void)
{
	int i = 0;
	CNode* pNode = m_pFirstNode;
#ifdef _DEBUG
	BOOL bIsValid = FALSE;
#endif

	// scorre la lista
	while(pNode!=(CNode*)NULL)
	{
#ifdef _DEBUG
		// controlla la signature
		bIsValid = CheckNode(pNode);
		ASSERTEXPR(bIsValid);
		if(!bIsValid)
			break;
#endif
		// controlla lo status
		if(pNode->status==USED_NODE)
			pNode->index = i++;

		// passa al nodo successivo
		pNode = pNode->next;
	}
}

/*
	FindFirstNode()

	Cerca il primo nodo della lista con lo status uguale a quello specificato.
*/
CNode* CNodeList::FindFirstNode(int status)
{
	CNode* pNode = m_pFirstNode;
#ifdef _DEBUG
	BOOL bIsValid = FALSE;
#endif

	// scorre la lista
	while(pNode!=(CNode*)NULL)
	{
#ifdef _DEBUG
		// controlla la signature
		bIsValid = CheckNode(pNode);
		ASSERTEXPR(bIsValid);
		if(!bIsValid)
		{
			pNode = (CNode*)NULL;
			break;
		}
#endif
		// se lo status e' diverso da quello specificato, passa al nodo successivo
		if(pNode->status!=status)
			pNode = pNode->next;
		else
			break;
	}

	return(pNode);
}

/*
	FindNextNode()

	Restituisce il puntatore al successivo nodo (occupato) della lista.
	Passare il puntatore al corrente, restituisce il puntatore al seguente.
*/
CNode* CNodeList::FindNextNode(CNode* pNode)
{
#ifdef _DEBUG
	BOOL bIsValid = FALSE;
#endif

	// scorre la lista (a partire dall'elemento corrente, ossia quello ricevuto come parametro)
	while(pNode!=(CNode*)NULL)
	{
#ifdef _DEBUG
		// controlla la signature
		bIsValid = CheckNode(pNode);
		ASSERTEXPR(bIsValid);
		if(!bIsValid)
		{
			pNode = (CNode*)NULL;
			break;
		}
#endif
		// posiziona sul nodo seguente (il puntatore sta' puntando al nodo corrente del chiamante)
		if((pNode = pNode->next)!=(CNode*)NULL)
		{
			// controlla lo status
			if(pNode->status==USED_NODE)
				break;
		}
	}

	return(pNode);
}

/*
	FindNodeByIndex()

	Cerca il nodo relativo all'indice.
*/
CNode* CNodeList::FindNodeByIndex(int index)
{
#ifdef _DEBUG
	BOOL bIsValid = FALSE;
#endif

	CNode* pNode = m_pFirstNode;

	// scorre la lista
	while(pNode!=(CNode*)NULL)
	{
#ifdef _DEBUG
		// controlla la signature
		bIsValid = CheckNode(pNode);
		ASSERTEXPR(bIsValid);
		if(!bIsValid)
		{
			pNode = (CNode*)NULL;
			break;
		}
#endif
		if(pNode->status==USED_NODE)
		{
			if(pNode->index==index)
				break;
		}
		
		pNode = pNode->next;
	}

	return(pNode);
}

/*
	GetAt()

	Restituisce il puntatore ai dati del nodo relativo all'indice.
*/
void* CNodeList::GetAt(int index)
{
	void* p = NULL;
	ITERATOR iter = FindAt(index);
	
	if(iter)
		if(iter->status==USED_NODE)
			p = iter->data;

	return(p);
}

/*
	ReleaseNode()

	Rilascia il nodo.
	Notare che i nodi non vengono mai rilasciati fisicamente, ma riciclati.
	Il rilascio consiste nel rilasciarne le risorse, reinizializzarlo e marcarlo come inutilizzato.
*/
BOOL CNodeList::ReleaseNode(CNode* pNode,int nMode)
{
	BOOL flag = FALSE;

	ASSERTEXPR(pNode);

	// controlla il puntatore
	if(pNode!=(CNode*)NULL)
	{
		// controlla lo status
		if(pNode->status==USED_NODE || pNode->status==REUSED_NODE)
		{
			flag = TRUE;

			// per permettere all'eventuale classe derivata di rilasciare le risorse associate al
			// nodo dei dati (ossia le risorse contenute in pNode->data)
			// notare che se i dati presenti in pNode->data sono o contengono una classe non e' sufficente
			// la delete di cui sotto, per cui, nella ridefinizione della virtuale, oltre ad eliminare
			// i dati contenuti in pNode->data bisogna eliminare anche pNode->data, effettuando i cast 
			// opportuni affinche venga chiamato il distruttore adeguato
			if(nMode==RELEASE_DELETE_MODE || nMode==RELEASE_ERASE_MODE)
			{
				if(!PreDelete(pNode))
				{
					// elimina il nodo di dati
					if(pNode->data)
						delete pNode->data;
				}
				
				// deve rimettere a NULL in modo tale che se il nodo viene riutilizzato, la chiamata alla
				// virtuale Initialize() generi automaticamente la creazione dei dati con la chiamata alla
				// virtuale Create()
				pNode->data = NULL;

				// marca il nodo come inutilizzato
				if(nMode==RELEASE_DELETE_MODE)
					InitializeNode(pNode,UNUSED_NODE,(void*)NULL);
				// marca il nodo come cancellato
				else if(nMode==RELEASE_ERASE_MODE)
					InitializeNode(pNode,ERASED_NODE,(void*)NULL);
			}
			else if(nMode==RELEASE_REMOVE_MODE)
			{
				// marca il nodo come inutilizzato
				InitializeNode(pNode,REUSED_NODE,Initialize(pNode->data));
			}

			m_nTot--;

			EnumerateNodes();
		}
	}

	return(flag);
}

/*
	ReleaseNodeList()

	Rilascia la lista dei nodi.
*/
void CNodeList::ReleaseNodeList(int nMode)
{
#ifdef _DEBUG
	BOOL bIsValid = FALSE;
#endif

	CNode* pNode = m_pFirstNode;
	CNode* pNextNode;

	// scorre la lista
	while(pNode!=(CNode*)NULL)
	{
#ifdef _DEBUG
		// controlla la signature
		bIsValid = CheckNode(pNode);
		ASSERTEXPR(bIsValid);
		if(!bIsValid)
		{
			pNode = (CNode*)NULL;
			break;
		}
#endif
		// salva l'indirizzo del nodo successivo
		pNextNode = pNode->next;

		// rilascia le risorse associate al nodo ed il nodo
		ReleaseNode(pNode,nMode);
		if(nMode==RELEASE_DELETE_MODE || nMode==RELEASE_ERASE_MODE)
			delete pNode;

		// passa al nodo successivo
		if(nMode==RELEASE_DELETE_MODE || nMode==RELEASE_ERASE_MODE)
			m_pFirstNode = pNode = pNextNode;
		else
			pNode = pNode->next;
	}

	// azzera il numero di elementi presenti
	if(nMode==RELEASE_DELETE_MODE || nMode==RELEASE_ERASE_MODE)
		m_nTot = 0;

	// resetta la lista
	if(nMode==RELEASE_DELETE_MODE || nMode==RELEASE_ERASE_MODE)
		m_pFirstNode = m_pLastNode = (CNode*)NULL;
}
