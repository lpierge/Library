/*$
	CTextFont.cpp
	Classe per la gestione del file words (wchg).
	Luca Piergentili, 13/12/25

	Vedi le note in CTextFont.h
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "algorithm.h"
#include "fastrand.h"
#include "window.h"
#include "win32api.h"
#include "csvlib.h"
#include "CNodeList.h"
#include "textdef.h"
#include "CTextFile.h"
#include "CTextFont.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

// per il caricamento dei nomi dei fonts installati nella lista
static CFontDataList* g_fontDataListPtr = NULL;
static int CALLBACK EnumFontFamExProc(const LOGFONTA*,const TEXTMETRICA	*,DWORD FontType,LPARAM lParam);

/*
	CTextFontManager()
*/
CTextFontManager::CTextFontManager()
{
	m_nCurrent = 0;
	g_fontDataListPtr = NULL;
}

/*
	Clean()
*/
void CTextFontManager::Clean(void)
{
	m_textFontList.DeleteAll();
	m_fontDataList.DeleteAll();
	memset(&m_defaultTextFont,'\0',sizeof(m_defaultTextFont));
	g_fontDataListPtr = NULL;
	m_nCurrent = 0;
}

/*
	Load()

	Carica la lista e l'elemento di default con i records del file words.
	Le linee che iniziano con ';' vengono considerate come commenti.
	Il record '[Default]' (etichetta case-sensitive) deve essere presente obbligatoriamente con valori validi.
	Il record '[Fonts]' (etichetta case-sensitive) e' opzionale:
	- se presente e con nomi font (separati da virgola), carica la lista con tali valori
	- se presente, ma con un valore nullo, carica la lista con tutti i fonts di sistema
	- se assente, usa il font specificato in [Default]

	Restituisce TRUE se carica correttamente dal file o FALSE altrimenti.
*/
BOOL CTextFontManager::Load(LPCSTR lpcszFilename,LPCSTR lpcszDefaultText)
{
	BOOL bRet = FALSE;
	BOOL bHasDefaultData = FALSE;
	BOOL bHasFontList = FALSE;
	BOOL bHasFontLabel = FALSE;
	CTextFile textFile;

	// apre il file specificato
	if(textFile.Open(lpcszFilename,FALSE))
	{
		char buffer[MAX_TF_RECORD_LEN+1] = {0};
		char* bufferPtr = NULL;
		BOOL bDefaultData = FALSE;
		TEXTFONTRECORD record = {0};

		// mappa il record del file words sulla struttura per leggere via formato CSV
		CSV_FIELD_MAP mapping[] = {
			{&record.text[0],sizeof(record.text)},
			{&record.fontname[0],sizeof(record.fontname)},
			{&record.fontsize[0],sizeof(record.fontsize)},
			{&record.fontcolor_r[0],sizeof(record.fontcolor_r)},
			{&record.fontcolor_g[0],sizeof(record.fontcolor_g)},
			{&record.fontcolor_b[0],sizeof(record.fontcolor_b)},
			{&record.textalign[0],sizeof(record.textalign)}
		};

		// inizializza Default e lista TEXTFONT (records)
		memset(&m_defaultTextFont,'\0',sizeof(m_defaultTextFont));
		m_defaultTextFont.fontcolor_r = m_defaultTextFont.fontcolor_g = m_defaultTextFont.fontcolor_b = 255;
		strcpyn(m_defaultTextFont.fontname,"Arial",sizeof(m_defaultTextFont.fontname));
		strcpyn(m_defaultTextFont.fontsize,"28",sizeof(m_defaultTextFont.fontsize));
		strcpyn(m_defaultTextFont.text,lpcszDefaultText,sizeof(m_defaultTextFont.fontsize));
		m_defaultTextFont.textalign = ALIGN_BOTTOM_LEFT;
		m_textFontList.EraseAll();

		// legge dal file words
		while(textFile.ReadLine(buffer,sizeof(buffer))!=FILE_EOF)
		{
			// salta i commenti
			if(buffer[0]==';')
				continue;

			// [Default] etichetta per TEXFONT (record) di default
			if(strncmp("[Default]",buffer,9)==0)
			{
				// salta la etichetta per leggere piu' sotto il record con csv_parse_line()
				char* p = buffer+9;
				while(p && *p && isspace(*p))
					p++;
				bufferPtr = p;
				bHasDefaultData = bDefaultData = TRUE;
			}
			// [Fonts] etichetta per lista fonts, se non e' un valore vuoto, carica la lista con i fonts 
			// indicati dall'utente, se invece il valore e' vuoto alla fine carichera' i fonts di sistema
			else if(strncmp("[Fonts]",buffer,7)==0)
			{
				bHasFontLabel = TRUE;
				char* p = buffer+7;
				while(p && *p && isspace(*p))
					p++;
				bHasFontList = EnumLabelFonts(p);

				// i dati di [Fonts] non sono un record TEXTFONT, quindi looppa a leggere la linea seguente
				continue;
			}
			// record utente (TEXTFONT)
			else
			{
				bufferPtr = buffer;
				bDefaultData = FALSE;
			}

			// analizza ogni linea del file words letta sopra come se fosse un CSV
			if(csv_parse_line(bufferPtr,mapping,sizeof(mapping)/sizeof(mapping[0]))==CSV_SUCCESS)
			{
				// salta le linee vuote
				if(!*record.text)
					continue;

				// verifica se (sopra) ha letto il record [Default] o un record utente				
				TEXTFONT* pTextfontPtr = NULL;
				if(bDefaultData)
				{
					pTextfontPtr = &m_defaultTextFont;
					bRet = TRUE;
				}
				else
				{
					TEXTFONT* textfont = (TEXTFONT*)m_textFontList.Add();
					if(textfont)
					{
						pTextfontPtr = textfont;
						bRet = TRUE;
					}
				}

				// carica i valori del record nell'elemento TEXTFONT e lo usa per il valore di default o lo inserisce nella lista
				if(pTextfontPtr)
				{
					strcpyn(pTextfontPtr->text,record.text,sizeof(pTextfontPtr->text));
					strcpyn(pTextfontPtr->fontname,record.fontname,sizeof(pTextfontPtr->fontname));
					strcpyn(pTextfontPtr->fontsize,record.fontsize,sizeof(pTextfontPtr->fontsize));
					pTextfontPtr->fontcolor_r = atoi(record.fontcolor_r);
					pTextfontPtr->fontcolor_g = atoi(record.fontcolor_g);
					pTextfontPtr->fontcolor_b = atoi(record.fontcolor_b);
					if(strcmp(record.textalign,"ALIGN_TOP_LEFT")==0)
						pTextfontPtr->textalign = ALIGN_TOP_LEFT;
					else if(strcmp(record.textalign,"ALIGN_TOP_RIGHT")==0)
						pTextfontPtr->textalign = ALIGN_TOP_RIGHT;
					else if(strcmp(record.textalign,"ALIGN_BOTTOM_LEFT")==0)
						pTextfontPtr->textalign = ALIGN_BOTTOM_LEFT;
					else if(strcmp(record.textalign,"ALIGN_BOTTOM_RIGHT")==0)
						pTextfontPtr->textalign = ALIGN_BOTTOM_RIGHT;
					else
						pTextfontPtr->textalign = ALIGN_RESERVED;
				}

				// solo il record [Default] deve essere completo, il record utente puo' specificare solo la frase, quindi, 
				// se il record contiene solo il testo della frase, per il resto assume i valori specificati per default
				if(!bDefaultData && !*pTextfontPtr->fontname)
				{
					strcpyn(pTextfontPtr->fontname,m_defaultTextFont.fontname,sizeof(pTextfontPtr->fontname));
					strcpyn(pTextfontPtr->fontsize,m_defaultTextFont.fontsize,sizeof(pTextfontPtr->fontsize));
					pTextfontPtr->fontcolor_r = m_defaultTextFont.fontcolor_r;
					pTextfontPtr->fontcolor_g = m_defaultTextFont.fontcolor_g;
					pTextfontPtr->fontcolor_b = m_defaultTextFont.fontcolor_b;
					pTextfontPtr->textalign = m_defaultTextFont.textalign;
				}
			}
		}

		textFile.Close();
	}

	// se e' stata specificata l'etichetta con valore vuoto, carica la lista con i nomi dei fonts installati nel sistema
	if(!bHasFontList && bHasFontLabel)
		bHasFontList = EnumInstalledFonts();

	// la linea con i valori di default deve essere presente obbligatoriamente
	if(!bHasDefaultData)
	{
		bRet = FALSE;
	}
	else // controlla che siano presenti tutti i valori
	{
		if(!*m_defaultTextFont.text)
			bRet = FALSE;
		if(!*m_defaultTextFont.fontname)
			bRet = FALSE;
		if(!*m_defaultTextFont.fontsize)
			bRet = FALSE;
		if(m_defaultTextFont.fontcolor_r + m_defaultTextFont.fontcolor_g + m_defaultTextFont.fontcolor_b > 765/*255 x 3*/) // max value: RGB(255,255,255)
			bRet = FALSE;
		if((int)m_defaultTextFont.textalign >= (int)ALIGN_RESERVED)
			bRet = FALSE;
	}

	return(bRet);
}

/*
	Next()

	Legge il prossimo elemento TEXTFONT della lista e ne restituisce il puntatore, ruotando sulla lista all'infinito.
	Se la lista e' vuota, restituisce il puntatore al TEXTFONT di default.
	Si suppone che non restituisca mai NULL.
*/
TEXTFONT* CTextFontManager::Next(void)
{
	TEXTFONT* textfont = NULL;
	int nTot = m_textFontList.Count();

	if(nTot <= 0)
	{
		return(Default());
	}
	else
	{
		textfont = (TEXTFONT*)m_textFontList.GetAt(m_nCurrent);
		if(++m_nCurrent >= nTot)
			m_nCurrent = 0;
	}
	
	return(textfont);
}

/*
	EnumLabelFonts()

	Riempie la lista dei nomi dei fonts usando la stringa dei nomi separati da virgola, presenti
	nel file words a seguire l'etichetta [Fonts].
*/
BOOL CTextFontManager::EnumLabelFonts(LPSTR pString)
{
	BOOL bRet = FALSE;

	// azzera la lista
	m_fontDataList.DeleteAll();

	// tokenizza la stringa per estrarre i nomi dei fonts separati da ','
    const char* delimiter = ",";
    char* token = strtok(pString,delimiter);

    while(token!=NULL)
	{
		// inserisce il nomde del font nella lista solo se gia' non esiste
		BOOL bFound = FALSE;
		ITERATOR iter;
		FONTDATA* pFontData;
		if((iter = m_fontDataList.First())!=(ITERATOR)NULL)
		{
			do
			{
				pFontData = (FONTDATA*)iter->data;
				if(pFontData)
				{
					if(strcmp(pFontData->name,token)==0)
					{
						bFound = TRUE;
						break;
					}
				}
				iter = m_fontDataList.Next(iter);
			} while(iter!=(ITERATOR)NULL);
		}
		if(!bFound)
		{
			FONTDATA* pFontData = (FONTDATA*)m_fontDataList.Add();
			if(pFontData)
			{
				bRet = TRUE;
				strcpyn(pFontData->name,token,sizeof(pFontData->name));
			}
		}

		token = strtok(NULL,delimiter);
    }

	return(bRet);
}

/*
	EnumInstalledFonts()

	Riempie la lista dei nomi dei fonts tramite la chiamata all'API Win32 e la callback relativa.
*/
BOOL CTextFontManager::EnumInstalledFonts(void)
{
    // handle del dispositivo di contesto per lo schermo, utilizzato 
	// come riferimento per l'enumerazione dei font
    HDC hdc = GetDC(NULL);
    if(hdc==NULL)
		return(FALSE);

    // struttura LOGFONT usata come filtro
    LOGFONTA lf = {0};

    // imposta il CharSet su DEFAULT_CHARSET per includere tutti i set di caratteri
    lf.lfCharSet = DEFAULT_CHARSET;
    
    // con il nome del font a NULL elenca tutte le famiglie
    lf.lfFaceName[0] = '\0';

    // inizializza il contatore dei fonts
    int fontCount = 0;

	// azzera la lista e resetta il ptr globale
	m_fontDataList.DeleteAll();
	g_fontDataListPtr = &m_fontDataList;

    // chiama la funzione di enumerazione specificando la callback e l'LPARAM usato 
	// come parametro personalizzato
    EnumFontFamiliesExA(hdc,&lf,(FONTENUMPROCA)EnumFontFamExProc,(LPARAM)&fontCount,0);

    ReleaseDC(NULL,hdc);

    return(TRUE);
}

/*
	EnumFontFamExProc()

	Callback per la enumerazione dei fonts di sistema, il prototipo deve corrispondere a ENUMFONTPROC.
	Carica la lista dei nomi dei fonts installati nel sistema.

	Restituire 0 per interrompere la enumerazione, !=0 per continuare fino alla fine.

	Notare che qui usa il puntatore (globale) alla lista per immagazzinare i nomi dei fonts presenti nel 
	sistema, per non dover implementare la funzione statica della classe, il passaggio del ptr this della 
	classe e tutto il resto bla, bla, bla, necessario per chiamare gestire una callback tramite il codice 
	della classe.
	A lo bruto.
*/
static int CALLBACK EnumFontFamExProc(	const LOGFONTA		*lpelfe,	// struttura LOGFONT (informazioni sul font logico)
										const TEXTMETRICA	*lpntme,	// struttura TEXTMETRIC (informazioni sulle metriche del font)
										DWORD				FontType,	// tipo di font (TrueType, Raster, ecc.)
										LPARAM				lParam		// parametro passato dall'applicazione (qui usa un contatore)
)
{
	// si assicura che il ptr globale alla lista sia stato fatto puntare alla lista membro della classe
	if(!g_fontDataListPtr)
		return(0);
	
    // lParam e' un puntatore al contatore dei font
    int *pCount = (int*)lParam;

    // considera solo i font scalabili (TrueType, OpenType, ecc.) e scarta il resto, scarta anche le varianti 
	// per scrittura verticale, ossia i nomi font che iniziano con il carattere '@' (lingue asiatiche)
    // notare che l'API di Win32 puo' elencare anche font raster e vettoriali legacy
	if((FontType & TRUETYPE_FONTTYPE) && lpelfe->lfFaceName[0]!='@')
    {
		// contatore dei font
		(*pCount)++;

		// determina lo stile principale dal peso
		const char *styleName = "Normale";
		if(lpelfe->lfWeight >= FW_HEAVY) {
			styleName = "Heavy";
		} else if (lpelfe->lfWeight >= FW_BOLD) {
			styleName = "Bold";
		} else if (lpelfe->lfWeight==FW_SEMIBOLD) {
			styleName = "Semi-Bold";
		} else if (lpelfe->lfWeight <= FW_LIGHT) {
			styleName = "Light";
		}

		// per stampare il nome del font, il peso e lo stile corsivo/italico, a titolo informativo, NON usato qui
/*		printf(	"%03d. Nome Famiglia: %-30s | Peso (Weight): %-4d (%s) | Corsivo (Italic): %s\n", 
				*pCount, 
				lpelfe->lfFaceName, 
				lpelfe->lfWeight,
				styleName,
				(lpelfe->lfItalic ? "si" : "no")
				);
*/

		// l'API di Win32 non restituisce solo i nomi delle famiglie dei fonts, ma ripete lo stesso nome per ogni 
		// variante (grassetto, italico, etc.) dello stesso font, quindi prima di inserire nella lista verifica che 
		// il nome del font non sia gia' presente
		BOOL bFound = FALSE;
		ITERATOR iter;
		FONTDATA* pFontData;
		if((iter = g_fontDataListPtr->First())!=(ITERATOR)NULL)
		{
			do
			{
				pFontData = (FONTDATA*)iter->data;
				if(pFontData)
				{
					if(strcmp(pFontData->name,lpelfe->lfFaceName)==0)
					{
						bFound = TRUE;
						break;
					}
				}
				iter = g_fontDataListPtr->Next(iter);
			} while(iter!=(ITERATOR)NULL);
		}
		if(!bFound)
		{
			FONTDATA* pFontData = (FONTDATA*)g_fontDataListPtr->Add();
			if(pFontData)
				strcpyn(pFontData->name,lpelfe->lfFaceName,sizeof(pFontData->name));
		}
    }
    
    return(1);
}

/*
	GetAFont()

	Ricava (in modo random) e restituisce un nome font dalla lista interna.
	Se la lista e' vuota o se si verifica un errore, restituisce il nome font specificato
	nell'elemento TEXTFONT di default.
*/
LPCSTR CTextFontManager::GetAFont(void)
{
	const char* pRandomFontName = m_defaultTextFont.fontname; // ricava il nome font di default
	int nTot = m_fontDataList.Count();

	// se la lista contiene elementi
	if(nTot > 0)
	{
		// genera un numero random e lo usa come indice per accedere alla 
		// lista, ricavando il nome del font
		int nRandom = (int)rand_w(0,nTot-1);
		FONTDATA* pTextFont = (FONTDATA*)m_fontDataList.GetAt(nRandom);
		if(pTextFont)
			pRandomFontName = pTextFont->name;
	}

	return(pRandomFontName);
}
