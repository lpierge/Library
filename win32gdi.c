/*$
	win32gdi.c
	Helpers e generiche relative al maneggio dell'interfaccia grafica.
	Luca Piergentili, Feb '26

	Attenzione: se si incorpora questo file (.c) in un progetto C++/MFC, la inclusione del file "window.h" di cui sotto
	genera, in cascata, la inclusione di "windows.h" e "afx.h", con quest'ultimo generando l'errore:
	#error MFC requires C++ compilation (use a .cpp suffix)
	dato che il file ha estensione .c.
	Per risolvere o si rinomina il file in .cpp o si forza la compilazione in modalita' C++ con l'opzione relativa (/TP).

	Note:

	Quando usare ReleaseDC() e quando usare invece DeleteDC():

	ReleaseDC
	Si usa esclusivamente per i DC "presi in prestito" dal sistema, tipicamente quelli legati a una finestra esistente.
	Funzioni accoppiate: GetDC o GetWindowDC.
	Logica: si sta chiedendo a Windows: "Fammi disegnare sulla tua finestra". Una volta terminato, bisogna "restituire" il permesso.
	Sintassi: ReleaseDC(hWnd, hdc); (richiede l'handle della finestra).

	DeleteDC
	Si usa per i DC "creati da zero" o "privati".
	Funzioni accoppiate: CreateDC, CreateDCA, CreateCompatibleDC, CreateIC.
	Logica: si alloca una nuova struttura dati nella memoria GDI. Windows non la rivuole indietro, ma vuole che venga distrutta per liberare RAM.
	Sintassi: DeleteDC(hdc); (non serve l'hWnd).

	Usando...			Bisogna usare...		Perche'?
	GetDC(hWnd)			ReleaseDC				Il DC appartiene alla finestra, si sta solo rilasciando.
	GetWindowDC(hWnd)	ReleaseDC				Come sopra, ma include i bordi/barra titolo.
	CreateDCA(...)		DeleteDC				E' stato creato un oggetto nuovo, va distrutto.
	CreateCompatibleDC	DeleteDC				E' stato creato un DC in memoria, va distrutto.
	BeginPaint			EndPaint				Eccezione: si usa solo dentro il messaggio WM_PAINT.

	Come regola generale: mai distruggere o rilasciare un DC se contiene un oggetto personalizzato (come un Font o una Bitmap) selezionato dentro.

	//$ terminare il controllo in tutto il codice relativamente a ReleaseDC e DeleteDC
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include "textdef.h"
#include "textedbmp.h"
#include "win32gdi.h"
#ifdef __cplusplus
  #include "CBinFile.h"
#endif

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
//#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	Compare...()

	Helpers per i calcoli delle aree dei rettangoli.
	
	Restituiscono:
	 1 = r1 maggiore
	-1 = r2 maggiore
	 0 = r1 uguale a r2
*/
/*
	CompareRectArea()
	
	Confronta le aree.
*/
int CompareRectArea(const RECT* r1,const RECT* r2)
{
	LONG area1 = (r1->right - r1->left) * (r1->bottom - r1->top);
	LONG area2 = (r2->right - r2->left) * (r2->bottom - r2->top);
    
	if(area1 > area2) return 1;
	if(area1 < area2) return -1;
	return 0;
}
/*
	CompareRectWidth()

	Confronta la larghezza.
*/
int CompareRectWidth(const RECT* r1,const RECT* r2)
{
	LONG w1 = r1->right - r1->left;
	LONG w2 = r2->right - r2->left;
	return (w1 > w2) ? 1 : (w1 < w2) ? -1 : 0;
}
/*
	CompareRectHeight()
	
	Confronta l'altezza.
*/
int CompareRectHeight(const RECT* r1,const RECT* r2)
{
	LONG h1 = r1->bottom - r1->top;
	LONG h2 = r2->bottom - r2->top;
	return (h1 > h2) ? 1 : (h1 < h2) ? -1 : 0;
}

/*
	NormalizeRect()

	Normalizza un RECT con valori negativi.
*/
void NormalizeRect(const RECT* r,RECT* rNormalized)
{
	rNormalized->left   = 0;
	rNormalized->top    = 0;
	rNormalized->right  = r->right  - r->left;
	rNormalized->bottom = r->bottom - r->top;
}

/*
	CalculateCoverDimensions()

	Calcola il ridimensionamento dell'immagine per adattarla all'area specificata (l'area del monitor) 
	in base alla modalita' fill/cover.

	Con fill/cover l'immagine deve coprire completamente l'area del monitor a partire dal lato minore 
	(in oriz. o vert.), quindi il lato opposto si ingigantisce o riduce, a seconda se l'immagine e' piu' 
	piccola o piu' grande, senza lasciare bande nere sullo schermo.

	In input le dimensioni dell'area da coprire e le dimensioni originali dell'immagine, in output le 
	dimensioni dell'immagine una volta ridimensionata.

	Restituisce 0 se i due RECT sono (gia') uguali, 1 altrimenti.
 */
int CalculateCoverDimensions(const RECT* rArea,const RECT* rImage,RECT* rResult)
{
	/* uguaglianza x coordinate */
	if(EqualRect(rArea,rImage))
	{
		*rResult = *rImage;
		return(0);
	}

	/* width e height dell'area da coprire */
	double viewW = (double)(rArea->right     - rArea->left);
	double viewH = (double)(rArea->bottom    - rArea->top);

	/* width e height dell'immagine originale */
	double imgW  = (double)(rImage->right  - rImage->left);
	double imgH  = (double)(rImage->bottom - rImage->top);

	/* fattore di scala (ratio) */
	double scaleX = viewW / imgW;
	double scaleY = viewH / imgH;
    
	/* per fill usa il ratio maggiore */
	double scale = (scaleX > scaleY) ? scaleX : scaleY;

	/* width e height (rect) dell'immagine ridimensionata */
	rResult->left   = 0;
	rResult->top    = 0;
	rResult->right  = (long)(imgW * scale);
	rResult->bottom = (long)(imgH * scale);

	return(1);
}

/*
    CalculateFitDimensions()
    
	Calcola il ridimensionamento dell'immagine per adattarla all'area specificata (l'area del monitor) 
	in base alla modalita' fit.

	Con fit l'immagine deve adattarsi all'area del monitor secondo il lato maggiore (in oriz. o vert.), 
	il che significa che il ridimensionamento lascera' comunque bande nere sullo schermo, sia che la
	immagine venga ingrandita o rimpicciolita.

	In input le dimensioni dell'area da coprire e le dimensioni originali dell'immagine, in output le 
	dimensioni dell'immagine una volta ridimensionata.

	Restituisce 0 se i due RECT sono (gia') uguali, 1 altrimenti.
*/
int CalculateFitDimensions(const RECT* rArea,const RECT* rImage,RECT* rResult)
{
	/* uguaglianza x coordinate */
	if(EqualRect(rArea,rImage))
	{
		*rResult = *rImage;
		return(0);
	}

    /* width e height dell'area da coprire (lo schermo) */
    double viewW = (double)(rArea->right     - rArea->left);
    double viewH = (double)(rArea->bottom    - rArea->top);

    /* width e height dell'immagine originale */
    double imgW  = (double)(rImage->right  - rImage->left);
    double imgH  = (double)(rImage->bottom - rImage->top);

    /* calcola i rapporti di scala per entrambi gli assi */
    double scaleX = viewW / imgW;
    double scaleY = viewH / imgH;
    
    /* per il fit usa il rapporto minore, garantendo che l'immagine non superi i confini dello schermo */
    double scale = (scaleX < scaleY) ? scaleX : scaleY;

    /* dimensioni dell'immagine ridimensionata */
    rResult->left   = 0;
    rResult->top    = 0;
    rResult->right  = (long)(imgW * scale);
    rResult->bottom = (long)(imgH * scale);

	return(1);
}

/*
	RemapTextPositionToFill() (*)

	Rimappa le coordinate a cui scrivere il testo nell'immagine in previsione del ridimensionamento dell'immagine da 
	parte di Windows secondo la modalita' fill.

	Qui l'algoritmo per rimappare le coordinate del testo nell'immagine ridimensionata (nel futuro), non si basa sul 
	fatto che l'immagine cresca o diminuisca, ma sul rapporto (ratio) tra l'immagine ridimensionata da Windows secondo 
	il metodo fill e quella originale.

	Da una parte abbiamo la dimensione dell'immagine ridimensionata (a partire dall'immagine originale, che puo' essere
	piu' piccola o piu' grande dell'area virtuale, fa lo stesso), cosi' come la calcolerebbe Windows, e dall'altra abbiamo 
	la dimensione dell'area virtuale.

	L'immagine ridimensionata deve essere per forza piu' grande dell'area virtuale perche' e' stato usato il modo fill e
	anche se fosse uguale non importerebbe per il calcolo della ratio.
	Quindi sottrae l'area virtuale all'area dell'immagine ridimensionata ed ottiene di quanto e' piu' grande tale immagine.
	A questo punto divide tale differenza per 2, per ripartirla sui due lati dato che Windows centra l'immagine, ed ottiene
	l'offset a cui riposizionare il testo nell'immagine originale.

	(*) usata back in the day quando si chiamava la funzione per scrivere testo nel bitmap con l'immagine alla risoluzione 
	originale, ma se l'immagine viene ridimensionata per evitare la deformazione/sfocatura del testo e le cappellate che puo' 
	fare Windows con la sua euristica di ri-dimensionamento/posizionamento, tale funzione perde la sua utilita' e quindi si 
	conserva qui SOLO COME ESEMPIO
*/
void RemapTextPositionToFill(const RECT* rArea,const RECT* rImage,const RECT* rImgScaled,TEXT_ALIGNMENT mode,int margin_x,int margin_y,long* outX,long* outY,double* ratio)
{
    /* dimensioni dello schermo virtuale */
    double viewW = (double)(rArea->right - rArea->left);
    double viewH = (double)(rArea->bottom - rArea->top);

    /* dimensioni dell'immagine originale */
    double imgW  = (double)(rImage->right - rImage->left);
    double imgH  = (double)(rImage->bottom - rImage->top);

    /* dimensioni dell'immagine ridimensionata (calcolate in base alla modalita' FILL) */
    double scalW = (double)(rImgScaled->right - rImgScaled->left);
    double scalH = (double)(rImgScaled->bottom - rImgScaled->top);

    /*
	calcola la differenza (in pixel) tra immagine ridimensionata e l'area virtuale
    sono sempre valori positivi o zero, perche' con FILL, per principio, l'immagine viene
	ridimensionata a maggiore, a meno che non sia esattamente uguale all'area virtuale
	*/
    double diffW = scalW - viewW;
    double diffH = scalH - viewH;

    /*
	calcola quanto (l'immagine ridimensionata) sfora (sull'area virtuale) per ogni lato (sinistra/destra o sopra/sotto)
    e divide la differenza per 2, perche' Windows centra l'immagine, ossia la differenza sta meta' e meta' sui due lati
	*/
    double offsetScalatoX = diffW / 2.0;
    double offsetScalatoY = diffH / 2.0;

    /*
	calcola il fattore di scala (ratio) tra l'immagine originale e quella ridimensionata ,da usare poi per calcolare dove spostare le coordinate
	es. area virtuale 3840 x 1080, immagine originale 2816 x 1536, immagine ridimensionata 3839 x 2094 -> ratio 1.36
	es. area virtuale 3840 x 1080, immagine originale 4096 x 1152, immagine ridimensionata 3840 x 1080 -> ratio 0.94
	divide poi la differenza (tra originale e ridim.) per il ratio ottenendo l'offset a cui per rimappare le coordinate
	*/
    *ratio = scalW / imgW;

    /* rimappa l'offset ed il margine (in scala) sull'immagine originale */
    double origOffsetX = offsetScalatoX / *ratio;
    double origOffsetY = offsetScalatoY / *ratio;
    double m_x = (double)margin_x / *ratio;
    double m_y = (double)margin_y / *ratio;

    /* posizionamento finale sull'immagine originale (orientamento top-down, lo standard per DIB) */
    switch (mode) {
        case ALIGN_TOP_LEFT:
            /* parte dall'inizio dell'area visibile (dopo l'offset di taglio) e aggiunge il margine */
            *outX = (long)(origOffsetX + m_x);
            *outY = (long)(origOffsetY + m_y);
            break;

        case ALIGN_TOP_RIGHT:
            /* parte dalla fine dell'area visibile e torna indietro del margine */
            *outX = (long)(imgW - origOffsetX - m_x);
            *outY = (long)(origOffsetY + m_y);
            break;

        case ALIGN_BOTTOM_LEFT:
            *outX = (long)(origOffsetX + m_x);
            *outY = (long)(imgH - origOffsetY - m_y);
            break;

        case ALIGN_BOTTOM_RIGHT:
            *outX = (long)(imgW - origOffsetX - m_x);
            *outY = (long)(imgH - origOffsetY - m_y);
            break;
		case ALIGN_CENTER:
			/* il centro geometrico della bitmap originale, in modalità FILL, il centro dell'immagine coincide sempre con il centro dell'area visibile */
			*outX = (long)(imgW / 2.0);
			*outY = (long)(imgH / 2.0);
			break;
    }
}

/*
	CalculateDisplayTextSize()

	Calcola, rispetto allo schermo, la dimensione in pixel del testo per sapere come mappare le coordinate
	(in pixel) dentro un immagine bitmap tramite la funzione di scrittura WriteTextOnBitmap().

	SOLO per font proporzionali (come Arial o Verdana) si potrebbe fare un calcolo empirico (approssimativo)
	usando il rapporto di default per il font, ossia una media empirica, come in:

		#define	FONT_TEXT_RATIO 0.6f
		double fontsize  = atof(28);
		int textW = (int)strlen("testo da scrivere") * (int)(fontsize * FONT_TEXT_RATIO);
		int textH = (int)fontsize;

	pero' occhio perche' e' una zozzeria.

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL CalculateDisplayTextSize(LPCSTR lpcszDeviceName,int nFontSize,LPCSTR lpcszFontName,LPCSTR lpcszText,int* nTextWidth,int* nTextHeight,SIZE* szPadding)
{
	BOOL bResult = FALSE;

	/* per trasformare il size del font in pixel, deve usare come riferimento il DC del monitor specificato
	in altra parole, NON  puo' usare GetDC(NULL) */
	HDC hdc = CreateDCA("DISPLAY",lpcszDeviceName,NULL,NULL);
	if(hdc)
	{
		HFONT hFont = CreateFont(	-(int)nFontSize,
									0,0,0,
									FW_NORMAL, 
									FALSE,FALSE,FALSE, 
									DEFAULT_CHARSET, 
									OUT_DEFAULT_PRECIS,
									CLIP_DEFAULT_PRECIS, 
									PROOF_QUALITY,
									DEFAULT_PITCH, 
									lpcszFontName);
		if(hFont)
		{
			HFONT hOldFont = (HFONT)SelectObject(hdc,hFont);

			/* calcola la dimensione di uno spazio da sommare eventualmente alla lunghezza 
			del testo, se al chiamante dovesse servire paddarlo a destra o sinistra */
			const char* pSpacing = {" "};
	
			/* dimensione del padding (1 spazio) */
			GetTextExtentPoint32(hdc,pSpacing,(int)strlen(pSpacing),szPadding);

			/* dimensione del testo */
			SIZE szText;
			GetTextExtentPoint32(hdc,lpcszText,(int)strlen(lpcszText),&szText);

#ifdef USE_TYPOGRAPHIC_POINTS
			/* DPI del DC (monitor) specificato */
			int dpiY = GetDeviceCaps(hdc,LOGPIXELSY);

			/* aggiusta le dimensioni del testo usando il rapporto (numero magico) tra i 72 DPI dei 
			punti tipografici ed i DPI del DC (monitor) specificato */
			*nTextWidth  = MulDiv(szText.cx,dpiY,72); 
			*nTextHeight = MulDiv(szText.cy,dpiY,72);

			/* al momento non usato... */
			int visual_adjustment = (int)(2 * (double)dpiY / 96.0); /* circa 2px a 100%, 3px a 125% */
#elif USE_PIXEL_POINTS
			*nTextWidth  = szText.cx; 
			*nTextHeight = szText.cy;
#endif
			bResult = TRUE;

			SelectObject(hdc,hOldFont);
			DeleteObject(hFont);
		}

		DeleteDC(hdc);
	}

	return(bResult);
}

/*
	CalculateDisplayTextPosition()

	Calcola le coordinate per il testo sull'immagine in pixel ed in modo assoluto, ossia a partire dallo 0,0 dell'immagine.
*/
BOOL CalculateDisplayTextPosition(TEXT_POSITION* textPosition,int textWidth,int textHeight,int nImageWidth,int nImageHeight,TASKBAR_INFO* taskbarInfo,SIZE* szPadding)
{
#if 0
	/* alternate usava questo */
	switch(textPosition->alignment) {
		case ALIGN_TOP_LEFT:
			textPosition->x = textPosition->margin_x;
			textPosition->y = textPosition->margin_y;
			break;
		case ALIGN_TOP_RIGHT:
			textPosition->x = nImageWidth - textPosition->margin_x - textWidth;
			textPosition->y = textPosition->margin_y;
			break;
		case ALIGN_BOTTOM_LEFT:
			textPosition->x = textPosition->margin_x;
			textPosition->y = nImageHeight - textPosition->margin_y - textHeight;
			break;
		case ALIGN_BOTTOM_RIGHT:
			textPosition->x = nImageWidth  - textPosition->margin_x - textWidth;
			textPosition->y = nImageHeight - textPosition->margin_y - textHeight;
			break;
		case ALIGN_CENTER:
			textPosition->x = (nImageWidth  / 2) - (textWidth / 2);
			textPosition->y = (nImageHeight / 2) - (textHeight / 2);
			break;
	}
#else
	/* tutto il resto usa questo */
	switch(textPosition->alignment) {
		case ALIGN_TOP_LEFT:
			if(taskbarInfo && szPadding)
				textPosition->x = textPosition->margin_x + ((taskbarInfo->orientation==TKB_LEFT) ? szPadding->cx : 0); /* correzione se testo inizia a sx e taskbar su stesso lato sx */
			else
				textPosition->x = textPosition->margin_x;
			textPosition->y = textPosition->margin_y;
			break;
		case ALIGN_TOP_RIGHT:
			if(taskbarInfo && szPadding)
				textPosition->x = nImageWidth - (textWidth + textPosition->margin_x + szPadding->cx);
			else
				textPosition->x = nImageWidth - (textWidth + textPosition->margin_x);
			textPosition->y = textPosition->margin_y;
			break;
		case ALIGN_BOTTOM_LEFT:
			if(taskbarInfo && szPadding)
				textPosition->x = textPosition->margin_x + ((taskbarInfo->orientation==TKB_LEFT) ? szPadding->cx : 0); /* correzione se testo inizia a sx e taskbar su stesso lato sx */
			else
				textPosition->x = textPosition->margin_x;
			textPosition->y = nImageHeight - (textHeight + textPosition->margin_y);
			break;
		case ALIGN_BOTTOM_RIGHT:
			if(taskbarInfo && szPadding)
				textPosition->x = nImageWidth -  (textWidth  + textPosition->margin_x + szPadding->cx);
			else
				textPosition->x = nImageWidth  - (textWidth + textPosition->margin_x);
			textPosition->y = nImageHeight - (textHeight + textPosition->margin_y);
			break;
		case ALIGN_CENTER:
			textPosition->x = (nImageWidth / 2) - (textWidth / 2);
			textPosition->y = (nImageHeight/ 2) - (textHeight / 2);
			break;
	}
#endif
	return(TRUE);
}

/*
	GetBiggerIconSize()
*/
#ifdef __cplusplus
int GetBiggerIconSize(LPCSTR lpcszIconFile,int nPreferredSize) /* versione C++ */
{
	ASSERTEXPR(lpcszIconFile);
	
	int nBiggerIcon = -1;
	BOOL bPreferredIcon = FALSE;
	ICONHEADER iconheader;
	ICONIMAGE* pIconImageArray;
	CBinFile f;
	if(f.OpenExistingReadOnly(lpcszIconFile))
	{
		if(f.Read(&iconheader,sizeof(ICONHEADER))!=FILE_EOF)
		{
			pIconImageArray = new ICONIMAGE[iconheader.nImageCount];
			if(pIconImageArray)
			{
				int i;
				for(i=0; i < iconheader.nImageCount; i++)
					if(f.Read((LPVOID)&pIconImageArray[i],sizeof(ICONIMAGE))!=FILE_EOF)
					{
						if(nPreferredSize!=0)
							if(pIconImageArray[i].nWidth==nPreferredSize)
								bPreferredIcon = TRUE;
						if(pIconImageArray[i].nWidth > nBiggerIcon)
							nBiggerIcon = pIconImageArray[i].nWidth;
					}
				delete [] pIconImageArray;
			}
		}
		f.Close();
	}
	if(nBiggerIcon < 0)
		nBiggerIcon = 32;
	if(bPreferredIcon)
		nBiggerIcon = nPreferredSize;
	
	return(nBiggerIcon);
}
#else
int GetBiggerIconSize(const char* lpcszIconFile,int nPreferredSize) /* versione C */
{
	ASSERTEXPR(lpcszIconFile);
	
    int nBiggerIcon = -1;
    int nPreferredIcon = 0;
    ICONHEADER iconheader;
    ICONIMAGE* pIconImageArray = NULL;

    FILE* f = fopen(lpcszIconFile,"rb");
    if(!f)
        return(-1);

    /* legge l'header dell'icona */
    if(fread(&iconheader,sizeof(ICONHEADER),1,f)==1)
	{
        if((pIconImageArray = (ICONIMAGE*)malloc(sizeof(ICONIMAGE) * iconheader.nImageCount))!=NULL)
		{
            /* legge tutte le ICONIMAGE in un'unica operazione */
            if(fread(pIconImageArray,sizeof(ICONIMAGE),iconheader.nImageCount,f)==iconheader.nImageCount)
			{
                for(int i=0; i < iconheader.nImageCount; i++)
				{
                    int nCurrentImageWidth = pIconImageArray[i].nWidth;

                    /* la dimensione 0 indica un'icona di 256 pixel */
                    if(nCurrentImageWidth==0)
                        nCurrentImageWidth = 256;

                    if(nPreferredSize!=0)
					{
						if (nCurrentImageWidth==nPreferredSize)
                            nPreferredIcon = 1;
                    }
                    
					if(nCurrentImageWidth > nBiggerIcon)
                        nBiggerIcon = nCurrentImageWidth;
                }
            }

            free(pIconImageArray);
        }
    }
    
    fclose(f);

    if(nBiggerIcon < 0)
        nBiggerIcon = 32;

    if(nPreferredIcon)
        nBiggerIcon = nPreferredSize;

    return(nBiggerIcon);
}
#endif

/*
	EnableHighDPI()

	SetProcessDPIAware() "certifica" l'app come consapevole dei DPI, e' l'alternativa all'uso del file manifest e 
	funziona a partire da Windows 8.1.
	Per un applicazione console non ha molto senso, qui viene riportata unicamente a scopo documentativo.
	
	Ad es., se la funzione NON viene chiamata su un laptop che ha uno scaling del 125% (molto comune sui laptop Full 
	HD), quando l'applicazione chiede al SO quale e' la risoluzione, Windows risponde: "1536x864" (ossia 1920 diviso 
	1.25). Il codice si basa quindi su 1536, ma i pixel reali dello schermo sono 1920 per cui se si prova a creare 
	una finestra o calcolare aree di disegno, i conti non torneranno mai.

	La funzione, o i suoi sostituiti, vedi sotto, va chiamata esattamente all'inizio del programma:

	//SetProcessDPIAware();															// antica
	//SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);	// solo a partire da Windows 10
	//EnableHighDPI();																// load dinamico
*/
typedef BOOL (WINAPI *PSETPROCESSDPIAWARENESSCONTEXT)(DPI_AWARENESS_CONTEXT);

void EnableHighDPI(void)
{
	HMODULE hUser32 = GetModuleHandleA("user32.dll");
	if(hUser32)
	{
		PSETPROCESSDPIAWARENESSCONTEXT pSetContext = (PSETPROCESSDPIAWARENESSCONTEXT)GetProcAddress(hUser32,"SetProcessDpiAwarenessContext");
        
		if(pSetContext)
		{
			/* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 e' (DPI_AWARENESS_CONTEXT)-4 */
			pSetContext((DPI_AWARENESS_CONTEXT)-4);
		}
		else
		{
			/* se non esiste (versione Windows incorretta), prova con l'API standard */
			SetProcessDPIAware();
		}
	}
}

/*
	GetMonitorResolution()

	Ricava la risoluzione del monitor (principale), versione corta.
*/
void GetMonitorResolution(UINT* nWidth,UINT* nHeight)
{
	*nWidth  = GetSystemMetrics(SM_CXSCREEN);
	*nHeight = GetSystemMetrics(SM_CYSCREEN);
}

/*
	GetPrimaryMonitorResolution()

	Ricava la risoluzione del monitor principale, versione estesa.
*/
void GetPrimaryMonitorResolution(MONITOR_RESOLUTION* pMonitorResolution)
{
	memset(pMonitorResolution,'\0',sizeof(MONITOR_RESOLUTION));

	/* risoluzione monitor principale */
	pMonitorResolution->screen_width  = GetSystemMetrics(SM_CXSCREEN);
	pMonitorResolution->screen_height = GetSystemMetrics(SM_CYSCREEN);

	/* DPI */
    HWND hwnd = GetDesktopWindow();
	if(hwnd)
	{
		HDC hdc = GetDC(hwnd);
		pMonitorResolution->dpi_x = GetDeviceCaps(hdc,LOGPIXELSX);
		pMonitorResolution->dpi_y = GetDeviceCaps(hdc,LOGPIXELSY);
	}

    /* risoluzione area virtuale tenendo conto dei DPI */
    pMonitorResolution->virtual_screen_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    pMonitorResolution->virtual_screen_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
}

/*
	GetVirtualMonitorArea()

	Determina l'area virtuale totale, formata dall'insieme dei monitor installati.

	Restituisce il numero di monitor presenti o -1 per errore.
 */
int GetVirtualMonitorArea(RECT* rVirtualArea)
{
	rVirtualArea->left   = 0;
	rVirtualArea->top    = 0;
	rVirtualArea->right  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	rVirtualArea->bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	int nTotMonitors = GetSystemMetrics(SM_CMONITORS);

	return((nTotMonitors > 0) ? nTotMonitors : -1);
}

/*
	EnumerateOneMonitor()

	Elenca il monitor specificato, vedi le note relative a EnumerateAllMonitors().
*/
BOOL EnumerateOneMonitor(int nMonitorProg,MONITOR_RESOLUTION* pMonitorResolution)
{
	/* ricava ed inizializza la struttura dove mettere le info */
	if(!pMonitorResolution)
		return(FALSE);
	ZeroMemory(pMonitorResolution,sizeof(MONITOR_RESOLUTION));

	/* ricava il numero totale di monitor presenti */
	int nMonitors = GetSystemMetrics(SM_CMONITORS);
	if(nMonitors < 1)
		return(FALSE);
	if(nMonitorProg < 1 || nMonitorProg > nMonitors)
		return(FALSE);

	/* imposta i dati per la callback in modo che questa ricerchi il monitor
	specificato tra tutti quelli presenti */
	ENUM_MONITOR_CONTEXT monitorData = {NULL,0,nMonitorProg,0,NULL};

	/* la funzione (EnumDisplayMonitors) enumera tutti i monitor, ma la callback 
	(EnumOneMonitorCallback) interrompe quando trova il monitor richiesto */
	EnumDisplayMonitors(NULL,NULL,EnumOneMonitorCallback,(LPARAM)&monitorData);

	/* e' ritornato ma la EnumOneMonitorCallback non ha trovato il monitor */
	HMONITOR hTarget = monitorData.hMonitor;
	if(!hTarget)
		return(FALSE);

	/* ricava le info sul monitor specificato */
	MONITORINFOEX monitorInfo;
	ZeroMemory(&monitorInfo,sizeof(MONITORINFOEX));
	monitorInfo.cbSize = sizeof(MONITORINFOEX);
	if(!GetMonitorInfo(hTarget,&monitorInfo))
		return(FALSE);

	/* handle del monitor */
	pMonitorResolution->hMonitor = hTarget;
	
	/* nome e numero device (monitor) */
	strcpyn(pMonitorResolution->deviceName,monitorInfo.szDevice,sizeof(pMonitorResolution->deviceName)/*MONITOR_DATA_DEVICE_NAME_LEN*/);
	char* pDisplay = strstr(monitorInfo.szDevice,"DISPLAY");
	if(pDisplay)
		pMonitorResolution->deviceNumber = atoi(pDisplay+7);

	/* area del monitor																				area di lavoro */
	pMonitorResolution->screen_width  = monitorInfo.rcMonitor.right  - monitorInfo.rcMonitor.left;	/*monitorInfo.rcWork.right - monitorInfo.rcWork.left;*/
	pMonitorResolution->screen_height = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;	/*monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;*/

	/* DPI del monitor (Win 8.1+) */
	HDC hdcScreen = GetDC(NULL);
	if(hdcScreen)
	{
		pMonitorResolution->dpi_x = GetDeviceCaps(hdcScreen,LOGPIXELSX);
		pMonitorResolution->dpi_y = GetDeviceCaps(hdcScreen,LOGPIXELSY);
		ReleaseDC(NULL,hdcScreen);
	}

	/* area virtuale (tutti i monitor) */
	pMonitorResolution->virtual_screen_width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	pMonitorResolution->virtual_screen_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	/* ricava se e' il monitor primario */
	pMonitorResolution->isPrimary = monitorInfo.dwFlags & MONITORINFOF_PRIMARY;

	/* offset del monitor rispetto al primario (0,0): quanto piu' a sx, piu' neg., quanto piu' a dx, piu' pos. */
	pMonitorResolution->offset = monitorInfo.rcMonitor.left;

	return(TRUE);
}

/*
	EnumOneMonitorCallback()

	Callback per EnumDisplayMonitors(), usata da EnumerateOneMonitor() per posizionarsi sul monitor
	specificato tra quelli esistenti.

	Viene chiamata per tutti i monitor, e quando trova una coincidenza con il monitor specificato, 
	interrompe il ciclo per segnalare al chiamante l'handle del monitor in questione.
*/
BOOL CALLBACK EnumOneMonitorCallback(HMONITOR hMonitor,HDC hdcMonitor,LPRECT lprcMonitor,LPARAM lParam)
{
	/* puntatore alla struttura contenente l'array e la sua dimensione attuale */
	ENUM_MONITOR_CONTEXT* pMonitorData = (ENUM_MONITOR_CONTEXT*)lParam;

	/* passa all'elemento seguente dell'array per verificare (piu' sotto) se e' quello in questione */
	pMonitorData->current++;

	/* scorre sull'array (ritornando TRUE) fino a quando interrompe (ritornando FALSE) perche' ha
	trovato il monitor in questione */
	if(pMonitorData->current==pMonitorData->target)
	{
		pMonitorData->hMonitor = hMonitor;
		return(FALSE);
	}
	
	return(TRUE);
}

/*
	EnumerateAllMonitors()

	Elenca i monitor fisicamente disponibili.

	Crea e riempie l'array con i dati relativi ad ogni monitor, agganciandolo al puntatore che riceve in input.
	Restituisce il numero di monitor presenti nel sistema, se tale valore e' > 0, allora il chiamante dovra' poi
	rilasciare la memoria allocata per l'array, chiamando la free() sul puntatore passato in input.

	La logica e' la seguente:
	EnumerateAllMonitors() chiama EnumDisplayMonitors() per elencare i monitor fisicamente disponibili, passandole 
	come callback la funzione EnumAllMonitorsCallback().
	EnumAllMonitorsCallback() chiama GetMonitorInfo() per ognuno degli handles ricevuti, funzione questa che ottiene
	l'area di lavoro ed il nome logico per ogni monitor (es. "\\.\DISPLAY1").
	Con tale nome logico viene poi chiamata la EnumDisplaySettings() per conoscere la modalita' video corrente del
	monitor in questione (risoluzione, frequenza, etc.).

	Il codice prende il puntatore originale per l'array, inizialmente a NULL, e lo alloca e rialloca per ogni monitor 
	trovato, quindi il chiamante, al ritorno da EnumerateAllMonitors(), deve rilasciare l'array allocato con free().

	Funziona a partire da Windows 8.1+.
*/
int EnumerateAllMonitors(MONITOR_DATA** ppMonitorData)
{
	/* puntatore alla struttura contenente l'array e la sua dimensione attuale */
	ENUM_MONITOR_CONTEXT monitorData = {NULL,0,0,0,NULL};

	/*
	scorre tutti i monitor esistenti popolando l'array

	OCCHIO: enumera i monitor in base alle coordinate del desktop virtuale (solitamente dall'alto a sinistra verso 
	il basso a destra), per cui se si spostano fisicamente i monitor nelle impostazioni di Windows, l'ordine di questa 
	enumerazione può cambiare
	*/
	if(!EnumDisplayMonitors(NULL,NULL,EnumAllMonitorsCallback,(LPARAM)&monitorData))
	{
		/* se errore, riazzera tutto */
		if(monitorData.pArray)
			free(monitorData.pArray);
		*ppMonitorData = NULL;
		return(-1);
	}

	/* imposta il puntatore del chiamante sull'array creato sopra */
	*ppMonitorData = monitorData.pArray;

	/* restituisce la dimensione dell'array, ossia il numero di monitor presenti */
	return(monitorData.count);
}

/*
	EnumAllMonitorsCallback()

	Callback per EnumDisplayMonitors(), usata da EnumerateAllMonitors() per ricavare le info relative
	a tutti i monitor installati.

	Viene chiamata per tutti i monitor, e per ognuno di essi ricava le informazioni relative.

	Il codice prende il puntatore originale per l'array, inizialmente a NULL, e lo alloca e rialloca
	per ogni monitor trovato, quindi il chiamante, al ritorno da EnumerateAllMonitors(), deve rilasciare
	l'array allocato con free().
*/
BOOL CALLBACK EnumAllMonitorsCallback(HMONITOR hMonitor,HDC hdcMonitor,LPRECT lprcMonitor,LPARAM lParam)
{
	/* puntatore alla struttura contenente l'array e la sua dimensione attuale */
	ENUM_MONITOR_CONTEXT* pMonitorData = (ENUM_MONITOR_CONTEXT*)lParam;

	/* aggiunge dinamicamente un elemento all'array */
	MONITOR_DATA* pTemp = (MONITOR_DATA*)realloc(pMonitorData->pArray,(pMonitorData->count + 1) * sizeof(MONITOR_DATA));
	if(!pTemp)
		return(FALSE);

	/* reimposta l'array */
	pMonitorData->pArray = pTemp;

	/* per i dati del monitor */
	MONITORINFOEX monitorInfo;
	ZeroMemory(&monitorInfo,sizeof(MONITORINFOEX));
	monitorInfo.cbSize = sizeof(MONITORINFOEX);

	/* imposta l'elemento dell'array appena creato con i dati relativi al monitor: */

	/* ricava i dati del monitor usando l'handle specificato */
	if(GetMonitorInfo(hMonitor,&monitorInfo))
	{
		pMonitorData->pArray[pMonitorData->count].hMonitor    = hMonitor;
		pMonitorData->pArray[pMonitorData->count].monitorArea = monitorInfo.rcMonitor;
		pMonitorData->pArray[pMonitorData->count].workingArea = monitorInfo.rcWork;
		pMonitorData->pArray[pMonitorData->count].isPrimary   = (monitorInfo.dwFlags & MONITORINFOF_PRIMARY);
		strcpyn(pMonitorData->pArray[pMonitorData->count].deviceName,monitorInfo.szDevice,sizeof(pMonitorData->pArray[pMonitorData->count].deviceName)/*MONITOR_DATA_DEVICE_NAME_LEN*/);
		char* pDisplay = strstr(monitorInfo.szDevice,"DISPLAY");
		if(pDisplay)
			pMonitorData->pArray[pMonitorData->count].deviceNumber = atoi(pDisplay+7);
        
		/* DEVMOD: area determinata dalla risoluzione corrente, ossia la modalita' video che la GPU sta' emettendo
		per il monitor in questione */
		DEVMODE dm;
		ZeroMemory(&dm,sizeof(DEVMODE));
		dm.dmSize = sizeof(DEVMODE);

		/* usa il nome del device ottenuto sopra con GetMonitorInfo per recuperare la modalita' video attiva
		EnumDisplaySettings() riempie DEVMODE con una delle modalita' video tra quelle che la scheda grafica supporta */
		if(EnumDisplaySettings(monitorInfo.szDevice,ENUM_CURRENT_SETTINGS,&dm))
		{
			/*
			area per la risoluzione corrente
			se il RECT di GetMonitorInfo() e' piu' piccolo dell'area della modalita' corrente di EnumDisplaySettings(),
			la differenza puo' essere causata da:
			- ingombo taskbar, side-bar, dock, etc.
			- scaling via DPI o zoom impostato dall’utente -> Impostazioni/Schermo/Scala e layout/125%, 150%, etc. per 
			  cui Windows aumenta la dimensione logica dei font/app, ma la GPU emette una risoluzione inferiore (ad es. 
			  1536x864 su un 1920x1080 fisico), per cui il rect (sopra) diventa piu' piccolo
			- scaling automatico per app non-DPI-aware -> se il processo e' marcato come non-DPI-aware, Windows aumenta 
			  il fattore  di scala e riduce la risoluzione virtuale per evitare blur
			- risoluzione virtuale bassa impostata dall’utente -> Impostazioni/Schermo/Risoluzione avanzata/1600x900 su 
			  un 1920x1080
			*/
			pMonitorData->pArray[pMonitorData->count].currentGPUModeWidth  = dm.dmPelsWidth;
			pMonitorData->pArray[pMonitorData->count].currentGPUModeHeight = dm.dmPelsHeight;
			pMonitorData->pArray[pMonitorData->count].freqHz               = dm.dmDisplayFrequency;
#if 0
			/* se dovesse servire recuperare il DPI effettivo per questo specifico monitor: */
			UINT dpiX,dpiY;
			/* MDT_EFFECTIVE_DPI restituisce il valore di scaling impostato dall'utente (es. 120 per 125%) */
			if(GetDpiForMonitor(hMonitor,MDT_EFFECTIVE_DPI,&dpiX,&dpiY)==S_OK)
			{
				/* calcola il fattore di scala (96 DPI e' il valore base 100%) */
				double scaleX = (double)dpiX / 96.0;
				double scaleY = (double)dpiY / 96.0;

				/*
				calcola la dimensione logica (quella usata da Windows per il Fill del Wallpaper)
				divide la risoluzione fisica per lo scaling del monitor
				*/
				pMonitorData->pArray[pMonitorData->count].logicWidth  = (long)(dm.dmPelsWidth / scaleX);
				pMonitorData->pArray[pMonitorData->count].logicHeight = (long)(dm.dmPelsHeight / scaleY);
			}
#endif
	}

		pMonitorData->count++;
	}

    return(TRUE);
}

/*
	EnumerateDisplayDevices()
	
	Elenca i devices disponibili interrogando il driver e l'hardware per sapere cosa puo' gestire 
	la scheda video.

	Crea e riempie l'array con i dati relativi ad ogni dispositivo, agganciandolo al puntatore che 
	riceve in input.

	Restituisce il numero di devices presenti nel sistema, se tale valore e' > 0, allora il chiamante 
	deve liberare la memoria allocata per l'array chiamando la free() sul puntatore passati in input.

	Il fatto che si vedano piu' dispositivi (\\.\DISPLAY1 etc.) rispetto ai monitor fisici e' un 
	comportamento normale in ambiente Windows quando l'hardware dispone di GPU integrate.
	Non si tratta necessariamente di "monitor virtuali" pronti all'uso, ma piuttosto di endpoint 
	logici che il driver video espone al sistema operativo.
*/
int EnumerateDisplayDevices(DISPLAY_DEVICE** ppDisplayDevice)
{
	DISPLAY_DEVICE* pArray = NULL;
	DISPLAY_DEVICE* pTemp  = NULL;
	DWORD nDevices = 0;
	BOOL bRet;

	for(int i = 0;; i++)
	{
		/* rialloca, ossia aggiunge un elemento all'array per i device, passato vuoto all'inizio */
		pTemp = (DISPLAY_DEVICE*)realloc(pArray,(nDevices + 1) * sizeof(DISPLAY_DEVICE));
		if(!pTemp)
		{
			if(pArray)
				free(pArray);
			*ppDisplayDevice = NULL;
			return -1;
		}
		pArray = pTemp;

		/* inizializza l'elemento dell'array e chiama EnumDisplayDevices */
		ZeroMemory(&pArray[nDevices],sizeof(DISPLAY_DEVICE));
		pArray[nDevices].cb = sizeof(DISPLAY_DEVICE);

		/* ricava le informazioni sul dispositivo */
		bRet = EnumDisplayDevices(NULL,i,&pArray[nDevices],0);
		if(!bRet)
			break;

		nDevices++;
	}

	/* imposta il puntatore del chiamante sull'array creato sopra */
	*ppDisplayDevice = pArray;

	/* restituisce la dimensione dell'array, ossia il numero di dispositivi presenti */
	return(nDevices);
}

/*
	GetMonitorIDFromGDIName()
	
	Cerca il "MonitorID" per l'oggetto COM partendo dal nome GDI (es. "\\.\DISPLAY1").
*/
BOOL GetMonitorIDFromGDIName(LPCSTR szGDIName,LPWSTR szOutMonitorID,UINT cchMax)
{
    UINT32 numPathArrayElements;
    UINT32 numModeInfoArrayElements;
    LONG status;

    /* chiede quanto spazio serve */
    GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&numPathArrayElements,&numModeInfoArrayElements);

    /* alloca lo spazio richiesto */
    DISPLAYCONFIG_PATH_INFO* pathArray = (DISPLAYCONFIG_PATH_INFO*)malloc(sizeof(DISPLAYCONFIG_PATH_INFO) * numPathArrayElements);
    DISPLAYCONFIG_MODE_INFO* modeInfoArray = (DISPLAYCONFIG_MODE_INFO*)malloc(sizeof(DISPLAYCONFIG_MODE_INFO) * numModeInfoArrayElements);

    /* ottiene la configurazione attuale */
    status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&numPathArrayElements,pathArray,&numModeInfoArrayElements,modeInfoArray,NULL);
    if(status!=ERROR_SUCCESS)
		return(FALSE);

    for(UINT32 i=0; i < numPathArrayElements; i++)
    {
        /* ottiene il nome della sorgente (GDI Name) */
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
        sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        sourceName.header.size = sizeof(sourceName);
        sourceName.header.adapterId = pathArray[i].sourceInfo.adapterId;
        sourceName.header.id = pathArray[i].sourceInfo.id;

        if(DisplayConfigGetDeviceInfo(&sourceName.header)==ERROR_SUCCESS)
        {
            WCHAR wszGDIName[32] = {0};
            MultiByteToWideChar(CP_ACP,0,szGDIName,-1,wszGDIName,32);

            if(wcscmp(sourceName.viewGdiDeviceName,wszGDIName)==0)
            {
                /* ha trovato il monitor, ora ricava qual'e' il suo target/destinazione (Device Path) */
                DISPLAYCONFIG_TARGET_DEVICE_NAME targetName;
                targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
                targetName.header.size = sizeof(targetName);
                targetName.header.adapterId = pathArray[i].targetInfo.adapterId;
                targetName.header.id = pathArray[i].targetInfo.id;

                if(DisplayConfigGetDeviceInfo(&targetName.header) == ERROR_SUCCESS)
                {
                    /* questo e' il valore che serve all'oggetto COM (monitorDevicePath) */
                    wcsncpy(szOutMonitorID,targetName.monitorDevicePath,cchMax);
                    free(pathArray);
                    free(modeInfoArray);
                    return(TRUE);
                }
            }
        }
    }

    free(pathArray);
    free(modeInfoArray);
    return(FALSE);
}

/*
	GetGDINameFromMonitorID()

	Riceve il monitorID (COM) e restituisce il nome GDI (es. \\.\DISPLAY1).
	Il parametro pnOutMonitorNumber per restituire l'indice numerico.
*/
BOOL GetGDINameFromMonitorID(LPCWSTR wszMonitorID,char* szOutGDIName,UINT cchMax,int* pnOutMonitorNumber)
{
    UINT32 numPathArrayElements;
    UINT32 numModeInfoArrayElements;
    LONG status;

    if(pnOutMonitorNumber)
		*pnOutMonitorNumber = -1;

    GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&numPathArrayElements,&numModeInfoArrayElements);

    DISPLAYCONFIG_PATH_INFO* pathArray = (DISPLAYCONFIG_PATH_INFO*)malloc(sizeof(DISPLAYCONFIG_PATH_INFO) * numPathArrayElements);
    DISPLAYCONFIG_MODE_INFO* modeInfoArray = (DISPLAYCONFIG_MODE_INFO*)malloc(sizeof(DISPLAYCONFIG_MODE_INFO) * numModeInfoArrayElements);

    status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&numPathArrayElements,pathArray,&numModeInfoArrayElements,modeInfoArray,NULL);

    if(status!=ERROR_SUCCESS)
	{
        free(pathArray);
		free(modeInfoArray);
        return(FALSE);
    }

    BOOL bFound = FALSE;
    for(UINT32 i=0; i < numPathArrayElements; i++)
    {
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName;
        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = pathArray[i].targetInfo.adapterId;
        targetName.header.id = pathArray[i].targetInfo.id;

        if(DisplayConfigGetDeviceInfo(&targetName.header)==ERROR_SUCCESS)
        {
            if(_wcsicmp(targetName.monitorDevicePath,wszMonitorID) == 0)
            {
                DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
                sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                sourceName.header.size = sizeof(sourceName);
                sourceName.header.adapterId = pathArray[i].sourceInfo.adapterId;
                sourceName.header.id = pathArray[i].sourceInfo.id;

                if(DisplayConfigGetDeviceInfo(&sourceName.header)==ERROR_SUCCESS)
                {
                    /* converte il nome GDI in ANSI (es. "\\.\DISPLAY1") */
                    WideCharToMultiByte(CP_ACP,0,sourceName.viewGdiDeviceName,-1,szOutGDIName,cchMax,NULL,NULL);
                    
                    /*
					estrae il numero, il nome e' nel formato "\\.\DISPLAY1", "\\.\DISPLAY2"...
                    cerca la parte numerica dopo "DISPLAY"
					*/
                    if(pnOutMonitorNumber)
                    {
                        const WCHAR* pDisplayKeyword = wcsstr(sourceName.viewGdiDeviceName,L"DISPLAY");
                        if(pDisplayKeyword!=NULL)
                            *pnOutMonitorNumber = _wtoi(pDisplayKeyword + 7);
                    }

                    bFound = TRUE;
                    break;
                }
            }
        }
    }

    free(pathArray);
    free(modeInfoArray);
    return(bFound);
}

/*
	IsTaskbarActuallyVisible()

	Indica se la taskbar e' attualmente visibile sullo schermo.
*/
BOOL IsTaskbarActuallyVisible(HWND hTaskbar)
{
	if(!IsWindowVisible(hTaskbar))
		return(FALSE);

	RECT rc = {0};;
	GetWindowRect(hTaskbar,&rc);

	int w = rc.right  - rc.left;
	int h = rc.bottom - rc.top;

	/* taskbar auto-hide collassata */
	if(w <= 2 || h <= 2)
		return(FALSE);

	return(TRUE);
}

/*
	GetTaskbarInfo()

	Ricava i dati relativi alla taskbar del monitor principale (primario) o di quello specificato.

	Se come handle della taskbar riceve NULL, ricava le info della taskbar principale, altrimenti
	ricava i dati della taskbar relativa all'handle.

	Attenzione perche' la zoccola di SHAppBarMessage() NON supporta le taskbar secondarie, per cui
	i dati che restituisce sono sempre e solo relativi alla alla taskbar primaria, per questo ricava 
	poi i dati delle secondarie direttamente e per differenza.
*/
BOOL GetTaskbarInfo(HWND hTaskbar,TASKBAR_INFO* pTaskbarInfo)
{

/* SHAppBarMessage (valido solo per primaria) */

    APPBARDATA appBarData = {0};
    appBarData.cbSize = sizeof(appBarData);

    /* handle della taskbar, se NULL ricava i dati della primaria */
	if(hTaskbar==NULL)
	{
	    memset(pTaskbarInfo,'\0',sizeof(TASKBAR_INFO));
		hTaskbar = FindWindow("Shell_TrayWnd",NULL);
		if(!hTaskbar)
			return(FALSE);
	}
    pTaskbarInfo->isVisible = IsWindowVisible(hTaskbar);
	appBarData.hWnd = hTaskbar;

    /* stato (auto-hide / always-on-top) */
    DWORD state = SHAppBarMessage(ABM_GETSTATE,&appBarData);
    pTaskbarInfo->isAutoHide    = (state & ABS_AUTOHIDE)!=0;
    pTaskbarInfo->isAlwaysOnTop = (state & ABS_ALWAYSONTOP)!=0;

    /* posizione e dimensione */
    if(SHAppBarMessage(ABM_GETTASKBARPOS,&appBarData))
    {
        pTaskbarInfo->rect   = appBarData.rc;
        pTaskbarInfo->width  = appBarData.rc.right  - appBarData.rc.left;
        pTaskbarInfo->height = appBarData.rc.bottom - appBarData.rc.top;

        switch(appBarData.uEdge)
        {
            case ABE_BOTTOM: pTaskbarInfo->orientation = TKB_BOTTOM; break;
            case ABE_TOP:    pTaskbarInfo->orientation = TKB_TOP;    break;
            case ABE_LEFT:   pTaskbarInfo->orientation = TKB_LEFT;   break;
            case ABE_RIGHT:  pTaskbarInfo->orientation = TKB_RIGHT;  break;
            default:         pTaskbarInfo->orientation = TKB_UNKNOWN;break;
        }
    }
    else
    {
        pTaskbarInfo->orientation = TKB_UNKNOWN;
    }

/* codice proprio (valido per tutte) */

	/* area taskbar */
	RECT rcTaskbar = {0};
	GetWindowRect(hTaskbar,&rcTaskbar);
	pTaskbarInfo->rect   = rcTaskbar;
    pTaskbarInfo->width  = rcTaskbar.right  - rcTaskbar.left;
    pTaskbarInfo->height = rcTaskbar.bottom - rcTaskbar.top;

	/* se sta' alla vista */
	pTaskbarInfo->isVisible = IsTaskbarActuallyVisible(hTaskbar);

	/* area del monitor associato alla taskbar */
	HMONITOR hMonitor = MonitorFromWindow(hTaskbar,MONITOR_DEFAULTTONULL);

	/* orientazione della taskbar */
	pTaskbarInfo->orientation = TKB_UNKNOWN;

	MONITORINFO monitorInfo = {0};
	monitorInfo.cbSize = sizeof(monitorInfo);
	if(GetMonitorInfo(hMonitor,&monitorInfo))
	{
		/*
		deduce l'orientazione
		il 200 e' un valore di soglia ottimo perche' per fattori come: DPI scaling (125%, 150%, 175%), auto-hide/animazioni, 
		o stili, temi, Explorer, etc. non sempre si verifica l'uguaglianza esatta tra il lato della taskbar e quello dello 
		schermo, come ad es. in: rcTaskbar.bottom==monitorInfo.rcMonitor.bottom
		200 e' piu' grande dell’altezza reale di una taskbar, piu' piccolo della dimensione di un monitor e quindi sicuro su 
		qualunque risoluzione, comprendo i valori tipici reali, come: taskbar orizzontale di 30–80 px, verticale di 40-120 px
		e "grande" di ~100 px
		oppure si calcola in base al monitor:
		int threshold = (monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top) / 10; // 10%
		*/
		int threshold = 200;
		if(rcTaskbar.bottom==monitorInfo.rcMonitor.bottom && rcTaskbar.top >= monitorInfo.rcMonitor.bottom - threshold)
			pTaskbarInfo->orientation = TKB_BOTTOM;
		else if(rcTaskbar.top==monitorInfo.rcMonitor.top && rcTaskbar.bottom <= monitorInfo.rcMonitor.top + threshold)
			pTaskbarInfo->orientation = TKB_TOP;
		else if(rcTaskbar.left==monitorInfo.rcMonitor.left)
			pTaskbarInfo->orientation = TKB_LEFT;
		else if(rcTaskbar.right==monitorInfo.rcMonitor.right)
			pTaskbarInfo->orientation = TKB_RIGHT;
	}

    return(TRUE);
}

/*
	EnumTaskbarProc()

	Callback usata da EnumerateAllTaskbars(), indirettamente dato che e' la callback che li' viene passata a 
	EnumWindows().

	Per ognuna delle finestre enumerate da EnumWindows(), controlla se si tratta delle finestre della taskbar 
	(primaria e secondarie), ricavando i dati relativi.

	Il codice prende il puntatore originale per l'array, inizialmente a NULL, e lo alloca e rialloca per ogni 
	taskbar trovata, quindi il chiamante, al ritorno da EnumerateAllTaskbars(), deve rilasciare l'array allocato 
	con free().
*/
BOOL CALLBACK EnumTaskbarProc(HWND hwnd, LPARAM lParam)
{
	/* ricerca in base al nome della classe a cui appartiene la finestra */
	char szWindowClassName[256] = {0};
	GetClassName(hwnd,szWindowClassName,sizeof(szWindowClassName));

	/* nomi taskbar principale: ""Shell_TrayWnd", secondarie: "Shell_SecondaryTrayWnd" */
	if(strcmp(szWindowClassName,"Shell_TrayWnd")==0 || strcmp(szWindowClassName,"Shell_SecondaryTrayWnd")==0)
	{
		/* puntatore alla struttura contenente l'array e la sua dimensione attuale */
		ENUM_TASKBAR_INFO* pTaskbarData = (ENUM_TASKBAR_INFO*)lParam;

		/* rialloca l'array aggiungendo un elemento */
		TASKBAR_INFO* pTemp = (TASKBAR_INFO*)realloc(*(pTaskbarData->pArray),(pTaskbarData->count + 1) * sizeof(TASKBAR_INFO));
		if(!pTemp)
			return(FALSE);

		/* aggiorna il puntatore originale del chiamante */
		*(pTaskbarData->pArray) = pTemp;

		/* posiziona sull'elemento appena aggiunto (l'ultimo dell'array) e solo dopo incrementa il contatore per il 
		numero di elementi per la prossima chiamata, tale contatore (count) deve iniziare da zero alla prima chiamata */
		TASKBAR_INFO* pCurrentElement = &pTemp[pTaskbarData->count];
		memset(pCurrentElement,'\0',sizeof(TASKBAR_INFO));
		pTaskbarData->count++;

		/* ora ricava le info sulla taskbar... */
		pCurrentElement->hWnd = hwnd;
		pCurrentElement->isTaskbarPrimary = (strcmp(szWindowClassName,"Shell_TrayWnd")==0);
		GetTaskbarInfo(pCurrentElement->hWnd,pCurrentElement);

		/* ...ed a partire dall'handle della taskbar, cerca il monitor associato, ricavando le info relative */
		if((pCurrentElement->hMonitor = MonitorFromWindow(hwnd,MONITOR_DEFAULTTONULL))!= NULL)
		{
			MONITORINFO monitorInfo = {0};
			monitorInfo.cbSize = sizeof(MONITORINFO);
			if(GetMonitorInfo(pCurrentElement->hMonitor,&monitorInfo))
				pCurrentElement->isRelatedMonitorPrimary = (monitorInfo.dwFlags & MONITORINFOF_PRIMARY);
		}
	}

	return(TRUE);
}

/*
	EnumerateAllTaskbars()

	Enumera tutte le taskbar presenti ricavando i dati nell'array relativo

	Dato che l'array con le info sulle taskbar viene allocato e riallocato dinamicamente, il chiamante
	da EnumerateAllTaskbars(), deve rilasciare l'array allocato con free().

	Restituisce il numero di taskbar presenti, ossia il totale di elementi nell'array.

	Esempio:

		TASKBAR_INFO* pTaskbarData = NULL;
		ENUM_TASKBAR_INFO taskbarData = {pTaskbarData,0};
		EnumerateAllTaskbars(&taskbarData);
		if(pTaskbarData)
			free(pTaskbarData);
*/
int EnumerateAllTaskbars(ENUM_TASKBAR_INFO* pTaskbarData)
{
	int nTotTaskbars = 0;

	if(pTaskbarData)
		if(EnumWindows(EnumTaskbarProc,(LPARAM)pTaskbarData))
			nTotTaskbars = pTaskbarData->count;

	return(nTotTaskbars);
}

/*
	SetWindowTransparency()

	Imposta il livello di trasparenza della finestra.
	I valori per 'alpha' vanno da 255 (completamente opaco) a 0 (praticamente invisibile).
	Vale solo per finestre proprie.
*/
BOOL SetWindowTransparency(HWND hWnd,BYTE alpha)
{
	BOOL bResult = FALSE;
	if(hWnd && (alpha >= 0 && alpha <= 255))
	{
		/* aggiunge lo stile layered se non presente */
		LONG exStyle = GetWindowLong(hWnd,GWL_EXSTYLE);
		if(!(exStyle & WS_EX_LAYERED))
			SetWindowLong(hWnd,GWL_EXSTYLE,exStyle | WS_EX_LAYERED);

		/* applica la trasparenza alla finestra */
		bResult = SetLayeredWindowAttributes(hWnd,0,alpha,LWA_ALPHA);
	}
	return(bResult);
}
