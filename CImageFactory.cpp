/*$
	CImageFactory.cpp
	Classe fattoria per l'oggetto immagine.
	Luca Piergentili, 10/05/00
	lpiergentili@yahoo.com

	Vedi le note in CImageFactory.h

	Ad memoriam - Nemo me impune lacessit.
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <string.h>
#include "strings.h"
#include "window.h"
#include "ImageLibraryName.h"
#include "ImageConfig.h"
#ifdef HAVE_PAINTLIB_LIBRARY
  #include "CPaintLib.h"
#endif
#ifdef HAVE_NEXGENIPL_LIBRARY
  #include "CNexgenIPL.h"
#endif
#ifdef HAVE_FREEIMAGE_LIBRARY
  #include "CFreeImage.h"
#endif
#ifdef HAVE_IMAGEMAGICK_LIBRARY
  #include "CImageMagick.h"
#endif
#include "CImageFactory.h"
#include "CImage.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CImageFactory()
*/
CImageFactory::CImageFactory()
{
	SUPPORTED_LIBRARIES* library = NULL;
	m_bCreated = FALSE;
	m_pImage = NULL;

	// aggiunge alla lista interna le librerie supportate
	// le macro HAVE_[...] vengono definite in ImageConfig.h per includere/escludere le librerie a compile-time

#ifdef HAVE_PAINTLIB_LIBRARY
	library = new SUPPORTED_LIBRARIES;
	if(library)
	{
		strcpyn(library->name,PAINTLIB_LIB_NAME,sizeof(library->name));
		m_librariesList.Add(library);
	}
#endif

#ifdef HAVE_NEXGENIPL_LIBRARY
	library = new SUPPORTED_LIBRARIES;
	if(library)
	{
		strcpyn(library->name,NEXGENIPL_LIB_NAME,sizeof(library->name));
		m_librariesList.Add(library);
	}
#endif

#ifdef HAVE_FREEIMAGE_LIBRARY
	library = new SUPPORTED_LIBRARIES;
	if(library)
	{
		strcpyn(library->name,FREEIMAGE_LIB_NAME,sizeof(library->name));
		m_librariesList.Add(library);
	}
#endif

#ifdef HAVE_IMAGEMAGICK_LIBRARY
	library = new SUPPORTED_LIBRARIES;
	if(library)
	{
		strcpyn(library->name,IMAGEMAGICK_LIB_NAME,sizeof(library->name));
		m_librariesList.Add(library);
	}
#endif
}

/*
	~CImageFactory()
*/
CImageFactory::~CImageFactory()
{
	Delete();
}

/*
	Create()

	Crea l'oggetto di tipo CImage a seconda della libreria specificata.
*/
CImage* CImageFactory::Create(LPSTR lpszLibraryName/* = NULL*/,UINT nSize/* = (UINT)-1*/)
{
	char* pLibraryName = lpszLibraryName ? lpszLibraryName : (char*)"";

	if(m_pImage)
		delete m_pImage;
	
	m_pImage = NULL;

	// le macro HAVE_[...] vengono definite in ImageConfig.h per includere/escludere le librerie a compile-time

#ifdef HAVE_PAINTLIB_LIBRARY
	if(!m_pImage)
		if(strcmp(pLibraryName,PAINTLIB_LIB_NAME)==0)
			m_pImage = new CPaintLib();
#endif
#ifdef HAVE_NEXGENIPL_LIBRARY
	if(!m_pImage)
		if(strcmp(pLibraryName,NEXGENIPL_LIB_NAME)==0)
			m_pImage = new CNexgenIPL();
#endif
#ifdef HAVE_FREEIMAGE_LIBRARY
	if(!m_pImage)
		if(strcmp(pLibraryName,FREEIMAGE_LIB_NAME)==0)
			m_pImage = new CFreeImage();
#endif
#ifdef HAVE_IMAGEMAGICK_LIBRARY
	if(!m_pImage)
		if(stricmp(pLibraryName,IMAGEMAGICK_LIB_NAME)==0)
			m_pImage = new CImageMagick();
#endif

#ifdef IMAGE_DEFAULT_LIBRARY
	if(!m_pImage)
	{
#ifdef HAVE_PAINTLIB_LIBRARY
		if(strcmp(IMAGE_DEFAULT_LIBRARY,PAINTLIB_LIB_NAME)==0)
			m_pImage = new CPaintLib();
#endif
#ifdef HAVE_NEXGENIPL_LIBRARY
		if(strcmp(IMAGE_DEFAULT_LIBRARY,NEXGENIPL_LIB_NAME)==0)
			m_pImage = new CNexgenIPL();
#endif
#ifdef HAVE_FREEIMAGE_LIBRARY
		if(strcmp(IMAGE_DEFAULT_LIBRARY,FREEIMAGE_LIB_NAME)==0)
			m_pImage = new CFreeImage();
#endif
#ifdef HAVE_IMAGEMAGICK_LIBRARY
		if(strcmp(IMAGE_DEFAULT_LIBRARY,IMAGEMAGICK_LIB_NAME)==0)
			m_pImage = new CImageMagick();
#endif
	}
#endif

	if(m_pImage && pLibraryName && nSize!=(UINT)-1)
		strcpyn(pLibraryName,m_pImage->GetLibraryName(),nSize);

	if(m_pImage)
		m_bCreated = TRUE;

	return(m_pImage);
}

/*
	Delete()
*/
void CImageFactory::Delete(void)
{
	if(m_bCreated)
		if(m_pImage)
			delete m_pImage,m_pImage = NULL;
}

/*
	EnumLibraryNames()

	Enumera le librerie supporate, da chiamare in un ciclo fino a che non restituisce NULL.
*/
LPCSTR CImageFactory::EnumLibraryNames(int& nIterator)
{
	char* pLibraryName = NULL;
	SUPPORTED_LIBRARIES* library;
	ITERATOR iter;

	if(nIterator < m_librariesList.Count())
	{
		if((iter = m_librariesList.FindAt(nIterator))!=(ITERATOR)NULL)
		{
			library = (SUPPORTED_LIBRARIES*)iter->data;
			if(library)
				pLibraryName = library->name;
		}
	}
	else
	{
		pLibraryName = NULL;
	}

	nIterator++;

	return(pLibraryName);
}
