/*
	CGdiImage.cpp
	Classe basica per gestione files grafici via GDI+.
	Luca Piergentili, 16/07/2026

	Note:
	I membri statici per l'avvio e la chiusura GDI+ vanno chiamati fuori dall'ambito di
	visibilita' dell'oggetto per la classe:

	ULONG_PTR ulToken = GdiImageHandler::Startup(void);
	{
		GdiImageHandler gdi;
		...
	}
	GdiImageHandler::Shutdown(ulToken);
*/
#include <stdlib.h>
#include <windows.h>
#include <gdiplus.h>
#include "CGdiImage.h"

/*
	Startup()
*/
ULONG_PTR GdiImageHandler::Startup(void)
{
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken = 0;
	Gdiplus::GdiplusStartup(&gdiplusToken,&gdiplusStartupInput,NULL);
	return(gdiplusToken);
}

/*
	Shutdown()
*/
void GdiImageHandler::Shutdown(ULONG_PTR token)
{
	if(token!=0)
		Gdiplus::GdiplusShutdown(token);
}

/*
	GdiImageHandler()
*/
GdiImageHandler::GdiImageHandler()
{
	m_pBitmap = NULL;
	m_pwszCurrentPath = NULL;
	m_nWidth = m_nHeight = 0;
}

/*
	~GdiImageHandler()
*/
GdiImageHandler::~GdiImageHandler()
{
	Free();
}

/*
	Free()
*/
void GdiImageHandler::Free(void)
{
	if(m_pBitmap)
	{
		delete m_pBitmap;
		m_pBitmap = NULL;
	}
	if(m_pwszCurrentPath)
	{
		delete[] m_pwszCurrentPath;
		m_pwszCurrentPath = NULL;
	}
}

/*
	Load()
*/
bool GdiImageHandler::Load(const wchar_t* pwszFilePath)
{
	Free();

	// carica dal file (il che imposta un lock esclusivo sul disco)
	Gdiplus::Bitmap* pTempBitmap = Gdiplus::Bitmap::FromFile(pwszFilePath);
	if(!pTempBitmap || pTempBitmap->GetLastStatus()!=Gdiplus::Ok)
	{
		if(pTempBitmap)
			delete pTempBitmap;
		return(false);
	}

	// per sbloccare il file, clona l'immagine disegnandola su una nuova Bitmap in memoria
	m_nWidth = pTempBitmap->GetWidth();
	m_nHeight = pTempBitmap->GetHeight();

	m_pBitmap = new Gdiplus::Bitmap(m_nWidth,m_nHeight,PixelFormat32bppARGB);
	if(m_pBitmap && m_pBitmap->GetLastStatus()==Gdiplus::Ok)
	{
		Gdiplus::Graphics graphics(m_pBitmap);
		graphics.DrawImage(pTempBitmap,0,0,m_nWidth,m_nHeight);
	}
	else
	{
		delete pTempBitmap;
		return(false);
	}

	// distrugge la bitmap originale, il file sul disco e' ora sbloccato
	delete pTempBitmap;

	// memorizza il path in caso venga chiamata Save() successivamente
	size_t pathLen = lstrlenW(pwszFilePath) + 1;
	m_pwszCurrentPath = new wchar_t[pathLen];
	lstrcpyW(m_pwszCurrentPath,pwszFilePath);

	return(true);
}

/*
	Resize()
*/
bool GdiImageHandler::Resize(UINT newWidth,UINT newHeight)
{
    if(!m_pBitmap)
		return(false);

	// crea una nuova bitmap vuota con le dimensioni target
	Gdiplus::Bitmap* pNewBitmap = new Gdiplus::Bitmap(newWidth,newHeight,PixelFormat32bppARGB);
	if(!pNewBitmap || pNewBitmap->GetLastStatus()!=Gdiplus::Ok)
	{
		if(pNewBitmap)
			delete pNewBitmap;
		return(false);
	}

	// disegna l'immagine vecchia su quella nuova
	Gdiplus::Graphics graphics(pNewBitmap);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.DrawImage(m_pBitmap,0,0,newWidth,newHeight);

	// scambia i puntatori e distrugge la vecchia
	delete m_pBitmap;
	m_pBitmap = pNewBitmap;

	return(true);
}

/*
	Save()
*/
bool GdiImageHandler::Save(void)
{
	if(!m_pBitmap || !m_pwszCurrentPath)
		return(false);
    
	const wchar_t* pwszMimeType = GetMimeTypeFromPath(m_pwszCurrentPath);

	return(SaveAs(m_pwszCurrentPath,pwszMimeType));
}

/*
	SaveAs()
*/
bool GdiImageHandler::SaveAs(const wchar_t* pwszFilePath,const wchar_t* pwszMimeType)
{
	if(!m_pBitmap)
		return(false);

	CLSID clsid;
	if(GetEncoderClsid(pwszMimeType,&clsid) < 0)
		return(false);

	Gdiplus::Status status = m_pBitmap->Save(pwszFilePath,&clsid,NULL);
	if(status==Gdiplus::Ok)
	{
		// se ha salvato con successo su un nuovo path, aggiorna il path interno (confronto case-insensitive)
		if(!m_pwszCurrentPath || lstrcmpiW(m_pwszCurrentPath,pwszFilePath)!=0)
		{
			if(m_pwszCurrentPath)
				delete[] m_pwszCurrentPath;

			size_t pathLen = lstrlenW(pwszFilePath) + 1;
			m_pwszCurrentPath = new wchar_t[pathLen];
			lstrcpyW(m_pwszCurrentPath,pwszFilePath);
		}
	
		return(true);
	}
    
	return false;
}

/*
	GetEncoderClsid()
*/
int GdiImageHandler::GetEncoderClsid(const wchar_t* format,CLSID* pClsid)
{
	UINT num = 0;   // numero totale encoders
	UINT size = 0;  // dimensione dell'array per gli encoders

	Gdiplus::GetImageEncodersSize(&num,&size);
	if(size==0)
		return(-1);

	Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if(pImageCodecInfo==NULL)
		return(-1);

	Gdiplus::GetImageEncoders(num,size,pImageCodecInfo);

	for(int j=0; j < (int)num; ++j)
	{
		if(lstrcmpW(pImageCodecInfo[j].MimeType,format)==0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return(j);
		}
	}
    
	free(pImageCodecInfo);

	return(-1);
}

/*
	GetMimeTypeFromPath()
*/
const wchar_t* GdiImageHandler::GetMimeTypeFromPath(const wchar_t* pwszPath)
{
	// usa come default un formato raw lossless

	if(!pwszPath)
		return(L"image/bmp");

	const wchar_t* ext = wcsrchr(pwszPath,L'.');
	if(ext)
	{
		if(lstrcmpiW(ext,L".png")==0)
			return(L"image/png");
		else if(lstrcmpiW(ext,L".jpg")==0 || lstrcmpiW(ext,L".jpeg")==0 || lstrcmpiW(ext,L".jpe")==0 || lstrcmpiW(ext,L".jfif")==0)
			return(L"image/jpeg");
		if(lstrcmpiW(ext,L".gif")==0)
			return(L"image/gif");
		if(lstrcmpiW(ext,L".tif")==0 || lstrcmpiW(ext, L".tiff")==0)
			return(L"image/tiff");
	}
    
	return(L"image/bmp");
}
