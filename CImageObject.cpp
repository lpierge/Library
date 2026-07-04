/*$
	CImageObject.cpp
	Classe base per la definizione dell'oggetto immagine.
	Luca Piergentili, 01/09/00
	lpiergentili@yahoo.com

	Vedi le note in CImageObject.h

	Ad memoriam - Nemo me impune lacessit.
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include "strings.h"
#include "window.h"
#include "CNodeList.h"
#include "CImageObject.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

// elenco dei tipi validi (riconosciuti come tali)
static const char* pImageFormatsArray[] = {
	".avi",	".bmp",	".cdr",	".cgm",	".cmx",
	".cur",	".dcx",	".dib",	".drw",	".dxf",
	".eps",	".flm",	".fpx",	".gif",	".icb",
	".ico",	".iif",	".img",	".jfif",".jif",
	".jpe",	".jpeg",".jpg",	".kdc",	".lbm",
	".mac",	".mpeg",".mp4",	".mpg",	".msp",
	".pbm",	".pcd",	".pct",	".pcx",	".pdd",
	".pdf",	".pgm",	".pic",	".pict",".png",
	".ppm",	".psd",	".psp",	".pxr",	".raw",
	".rle",	".sct",	".tga",	".tif",	".tiff",
	".vda",	".vst",	".wmf"
};

/*
	CImageObject()
*/
CImageObject::CImageObject()
{
	memset(m_szError,'\0',sizeof(m_szError));
	memset(m_szFileName,'\0',sizeof(m_szFileName));
	memset(m_szFormat,'\0',sizeof(m_szFormat));
	memset(&m_InfoHeader,'\0',sizeof(m_InfoHeader));

	// array per i valori (min,max,default) dei filtri e la funzione che li esegue
	// mantenere aggiornato il dimensionamento in CImageObject.h
	// si compone di nome funzione, valori min e max e valore di default, se il filtro e' nativo della libreria o implementato nella 
	// classe madre ed il puntatore (C++) alla funzione per eseguirlo
	// le derivate devono modificare l'array chiamando la SetFilterParams() relativa al filtro che definiscono con i relativi parametri
	// -1,-1,-1 = funzione non prevista
	//  0, 0, 0 oppure "","" = funzione senza valori (min,max,default)
	// la maggior parte dei filtri usa un solo parametro, quello presente nella colonna -1,-1,-1 per i filtri che necessitano piu' di un
	// parametro, usare la colonna "","" separando gli intervalli con ':' ed i parametri con ';'
	// il puntatore alla funzione viene specificato in modo polimorfico e va poi referenziato tenendo conto la chiamata avverra' tramite
	// la v-table: FILTER_FUNCTION (nel .h) definisce il tipo per il puntatore alla funzione, una volta ricavato tale puntatore dall'array 
	// tramite la GetFilterFunction(), la chiamata va fatta con la seguente sintassi: (pImage->*fn)(); con pImage=oggetto e fn=ptr alla 
	// funzione, notare anche che nei confronti con il ptr non si puo' usare NULL ma deve essere usato nullptr
	m_filterParams[0]  = {"Blur",			-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT | FLTR_TYPE_PERF_LOW};
	m_filterParams[1]  = {"Brightness",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[2]  = {"ColorShift",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[3]  = {"ColorSwap",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[4]  = {"Contrast",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[5]  = {"Echo",			-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT | FLTR_TYPE_PERF_LOW};
	m_filterParams[6]  = {"EdgeEnhance",	-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT};
	m_filterParams[7]  = {"Emboss",			-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT};
	m_filterParams[8]  = {"Equalize",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[9]  = {"FindEdge",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT};
	m_filterParams[10] = {"GammaCorrection",-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[11] = {"GhostTrail",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT | FLTR_TYPE_PERF_LOW};
	m_filterParams[12] = {"Grain",			-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[13] = {"Grayscale",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR | FLTR_TYPE_BLACKWHITE};
	m_filterParams[14] = {"HalftoneBW",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT | FLTR_TYPE_BLACKWHITE};
	m_filterParams[15] = {"HalftoneColor",	-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT};
	m_filterParams[16] = {"Hue",			-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[17] = {"Intensity",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[18] = {"JitterHorizontal",-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_GEOM_ISO};
	m_filterParams[19] = {"JitterSinusoidal",-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_GEOM_ISO};
	m_filterParams[20] = {"Median",			-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT | FLTR_TYPE_PERF_LOW};
	m_filterParams[21] = {"MirrorHorizontal",-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_GEOM_ISO};
	m_filterParams[22] = {"MirrorVertical",	-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_GEOM_ISO};
	m_filterParams[23] = {"Negate",			-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[24] = {"Noise",			-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[25] = {"Pixelate",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT};
	m_filterParams[26] = {"PixelSort",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT | FLTR_TYPE_PERF_LOW};
	m_filterParams[27] = {"Posterize",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[28] = {"Rotate90Left",	-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_GEOM_RATIO | FLTR_TYPE_GEOM_RESIZE};
	m_filterParams[29] = {"Rotate90Right",	-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_GEOM_RATIO | FLTR_TYPE_GEOM_RESIZE};
	m_filterParams[30] = {"Rotate180",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_GEOM_ISO};
	m_filterParams[31] = {"Saturation",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR};
	m_filterParams[32] = {"Sharpen",		-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_STRUCT};
	m_filterParams[33] = {"Test",			-1,-1,-1,	"","",FALSE,nullptr,FLTR_TYPE_COLOR | FLTR_TYPE_STRUCT | FLTR_TYPE_GEOM_ISO | FLTR_TYPE_PERF_LOW};
}

/*
	SetFilterParams()

	Imposta i valori relativi al filtro specificato.
	La funzione puo' essere chiamata in due modi:
	- per impostare i valori min, max, default, etc.
	- per cambiare il valore (dentro la scala min/max), da usare quando il chiamante vuole usare il filtro con valori personalizzati
*/
BOOL CImageObject::SetFilterParams(LPCSTR lpcszFilterName,double nMin,double nMax,double nDefault,const char* szRanges,const char* szValues,TERN tNative,FILTER_FUNCTION fn)
{
	BOOL bSet = FALSE;

	// cerca il filtro x nome (case sensitive)
	for(int i = 0; i < ARRAY_SIZE(m_filterParams); i++)
		if(strcmp(lpcszFilterName,m_filterParams[i].function)==0)
		{
			// se min e max == -1, allora sta cambiando il valore da usare per il filtro
			// se min e max != -1, allora sta impostando i valori min, max e default
			if(nMin!=-1 && nMax!=-1)
			{
				m_filterParams[i].min = nMin;
				m_filterParams[i].max = nMax;
			}
			// valore di default
			m_filterParams[i].value = nDefault;

			// se il filtro prevede piu' parametri, vanno passati con una stringa, separati da ;
			if(szRanges)
				strcpyn(m_filterParams[i].minmax,szRanges,sizeof(m_filterParams[i].minmax));
			if(szValues)
				strcpyn(m_filterParams[i].values,szValues,sizeof(m_filterParams[i].values));

			if(tNative!=Undef)
				m_filterParams[i].native = tNative;	// se il filtro e' supportato nativamente dalla libreria o dalla classe madre

			if(fn!=nullptr)
				m_filterParams[i].filter = fn;		// puntatore (polimorfico) alla funzione che implementa il filtro

			bSet = TRUE;
			break;
		}

	return(bSet);
}

/*
	SetFilterParams()

	Imposta SOLO il valore da usare con il filtro, NON imposta la scala di valori per min, max, default, etc.
*/
BOOL CImageObject::SetFilterParams(LPCSTR lpcszFilterName,double nValue,const char* szValues/* = NULL*/)
{
	TERN tNative = Undef;
	return(SetFilterParams(lpcszFilterName,-1.0,-1.0,nValue,NULL,szValues,tNative,nullptr));
}

/*
	GetFilterParams()

	Ricava il valore da usare come parametro per il filtro.

	I valori vengono impostati con SetFilterParams(), che puo' essere chiamata in due modi: o per 
	impostare il valore di default o per impostare il valore personalizzato del chiamante.

	Restituisce TRUE se trova il valore per il filtro e tale valore rientra nella scala prevista,
	FALSE altrimenti.
*/
BOOL CImageObject::GetFilterParams(LPCSTR lpcszFilterName,double& nValue,char* szValues/* = NULL*/,size_t nValuesSize/* = 0*/)
{
	// cerca il nome del filtro (case sensitive) ed imposta il valore del parametro
	for(int i = 0; i < ARRAY_SIZE(m_filterParams); i++)
		if(strcmp(lpcszFilterName,m_filterParams[i].function)==0)
		{
			// se sta' tutto a -1, allora il filtro non e' implementato
			if(m_filterParams[i].min==-1 && m_filterParams[i].max==-1 && m_filterParams[i].value==-1)
				return(FALSE);

			// filtro con + parametri
			if(*(m_filterParams[i].values))
			{
				if(szValues)
				{
					strcpyn(szValues,m_filterParams[i].values,nValuesSize);
					return(TRUE);
				}
				else
					return(FALSE);
			}
			else // filtro con 1 solo parametro
			{
				// controlla se il valore rientra nel range consentito
				if(m_filterParams[i].value >= m_filterParams[i].min && m_filterParams[i].value <= m_filterParams[i].max)
				{
					nValue = m_filterParams[i].value;
					return(TRUE);
				}
				else
				{
					nValue = 0.0f;
					return(FALSE);
				}
			}

			break; // non dovrebbe mai arrivare qui...
		}

	return(FALSE);
}

/*
	GetFilterParams()

	Ricava i valori per il range dei parametri per il filtro.
*/
BOOL CImageObject::GetFilterParams(LPCSTR lpcszFilterName,double& nMin,double& nMax,double& nDefault,char* szMinMax,size_t nMinMaxSize,char* szDefault,size_t nDefaultSize,BOOL& bNative)
{
	// cerca il nome del filtro (case sensitive) e restituisce il range per il parametro
	for(int i = 0; i < ARRAY_SIZE(m_filterParams); i++)
		if(strcmp(lpcszFilterName,m_filterParams[i].function)==0)
		{
			// se sta' tutto a -1, allora il filtro non e' implementato
			if(m_filterParams[i].min==-1 && m_filterParams[i].max==-1 && m_filterParams[i].value==-1)
				return(FALSE);

			// se il filtro e' supportato nativamente dalla libreria o dalla classe madre
			bNative = m_filterParams[i].native;

			// filtro con + parametri
			if(*(m_filterParams[i].values))
			{
				if(szMinMax)
				{
					strcpyn(szMinMax,m_filterParams[i].minmax,nMinMaxSize);
					strcpyn(szDefault,m_filterParams[i].values,nDefaultSize);
					return(TRUE);
				}
				else
					return(FALSE);
			}
			else // filtro con 1 solo parametro
			{
				nMin = m_filterParams[i].min;
				nMax = m_filterParams[i].max;
				nDefault = m_filterParams[i].value;
				return(TRUE);
			}

			break; // non dovrebbe mai arrivare qui...
		}

	return(FALSE);
}

/*
	GetFilterFunction()

	Versione per ricavare il puntatore (polimorfico) alla funzione per il filtro.
*/
FILTER_FUNCTION CImageObject::GetFilterFunction(LPCSTR lpcszFilterName,BYTE& bitmask)
{
	// cerca il nome del filtro (case sensitive) e restituisce il puntatore alla funzione
	for(int i = 0; i < ARRAY_SIZE(m_filterParams); i++)
		if(strcmp(lpcszFilterName,m_filterParams[i].function)==0)
		{
			bitmask = m_filterParams[i].filterbitmask;
			return(m_filterParams[i].filter);
		}

	bitmask = 0;
	return(nullptr);
}

/*
	IsFilterImplemented()

	Controlla se il filtro e' implementato (nome filtro case sensitive).
*/
BOOL CImageObject::IsFilterImplemented(LPCSTR lpcszFilterName)
{
	double nValue = 0.0f;
	return(GetFilterParams(lpcszFilterName,nValue));
}

/*
	NormalizeParamValue()

	Normalizza un valore da una scala sorgente a una scala destinazione.

	Parametri:
		baseMin    inizio scala basica (es. 0)
		baseMax    fine scala basica (es. 10)
		val        valore scelto nella scala basica (es. 5)
		filterMin  inizio scala filtro (es. -100)
		filterMax  fine scala filtro (es. +100)

	Restituisce il valore normalizzato per la scala del filtro.
 */
double CImageObject::NormalizeParamValue(double baseMin,double baseMax,double val,double filterMin,double filterMax)
{
    // protezione x divisione per zero se la scala base e' definita male
    if (baseMax == baseMin) return filterMin;

    // calcola la posizione relativa (da 0.0 a 1.0) nel range sorgente
    double relativePos = (val - baseMin) / (baseMax - baseMin);

    // proietta la posizione relativa nel range di destinazione
    double result = filterMin + (relativePos * (filterMax - filterMin));

    return result;
}

/*
	IsImageFile()

	Controlla (in base all'estensione) se il nome file fa riferimento ad un tipo valido.
*/
BOOL CImageObject::IsGraphicsFormat(LPCSTR lpcszFileName)
{
	// formato grafico, non necessariamente supportato
	for(int i = 0; i < ARRAY_SIZE(pImageFormatsArray); i++)
		if(striright(lpcszFileName,pImageFormatsArray[i])==0)
			return(TRUE);

	return(FALSE);
}

/*
	IsSupportedFormat()

	Controlla se si tratta di un formato supportato.
	Il controllo puo' avvenire con il nome del file (es. "image.jpg"), con la
	sola estensione (es. ".jpg") o con una stringa identificativa (es. "JPG").
*/
BOOL CImageObject::IsSupportedFormat(LPCSTR lpcszFileName)
{
	BOOL bSupported = FALSE;
	const char* pExt = (char*)strrchr(lpcszFileName,'.');
	BOOL bIsAnExtension = TRUE;
	if(!pExt)
	{
		pExt = lpcszFileName;
		bIsAnExtension = FALSE;
	}

	LPIMAGETYPE p;
	ITERATOR iter;
	if((iter = m_ImageTypeList.First())!=(ITERATOR)NULL)
	{
		do
		{
			p = (LPIMAGETYPE)iter->data;
			if(p)
				if(stricmp(pExt,bIsAnExtension ? p->ext : p->ext+1)==0)
				{
					bSupported = TRUE;
					break;
				}

			iter = m_ImageTypeList.Next(iter);
			
		} while(iter!=(ITERATOR)NULL);
	}

	return(bSupported);
}

/*
	CountSupportedFormats()

	Restituisce il numero di formati supportati.
*/
int CImageObject::CountSupportedFormats(void)
{
	return(m_ImageTypeList.Count());
}

/*
	EnumSupportedFormats()

	Enumera i formati supporati, da chiamare in un ciclo fino a che non restituisca	NULL.

	Esempio:
	int nIterator = 0;
	LPIMAGETYPE pImagetype;
	while((pImagetype = EnumSupportedFormats(nIterator))!=NULL)
		printf("file extension: %s\n",pImagetype->ext);
*/
LPIMAGETYPE CImageObject::EnumSupportedFormats(int& nIterator)
{
	LPIMAGETYPE p = NULL;
	ITERATOR iter;

	if(nIterator >= m_ImageTypeList.Count())
	{
		nIterator = 0;
		p = NULL;
	}
	else
	{
		if((iter = m_ImageTypeList.FindAt(nIterator))!=(ITERATOR)NULL)
		{
			p = (LPIMAGETYPE)iter->data;
			nIterator++;
		}
	}

	return(p);
}
