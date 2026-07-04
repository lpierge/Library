/*$
	CImageMagick.cpp
	Classe derivata per l'interfaccia con la libreria ImageMagick.
	Luca Piergentili, 20/01/26

	Vedi le note in CImageMagick.h

	TODO:

	Varie cose da aggiustare:

	nei filtri inserire tale codice:

	// se modificati i pixel con SetPixel(), riportarli nel Wand
    if (m_bDataModified) {
        SyncBufferToMagickWand();
        m_bDataModified = FALSE;
    }

	prima dell'elaborazione, come in:

	BOOL CImageMagick::Blur(double radius, double sigma)
	{
		if (!m_pWand) return FALSE;

		// se modificati i pixel con SetPixel(), riportarli nel Wand
		if (m_bDataModified) {
			SyncBufferToMagickWand();
			m_bDataModified = FALSE;
		}

		// ora ImageMagick puo' lavorare sui dati corretti
		if (MagickBlurImage(m_pWand, radius, sigma) == MagickFalse)
			return FALSE;

		// DOPO il filtro, il buffer C++ e' "invecchiato"
		// lo invalida cosi' la prossima GetPixel o Sync lo ricarichera' dal Wand
		InvalidateBuffer(); // funzione che mette m_pPixelBuffer = NULL o forza il refresh
		return TRUE;
	}

	La Save deve sincronizzare ALL'INIZIO

	BOOL CImageMagick::Save(const char* szPath)
	{
		if (!m_pWand) return FALSE;

		// se ci sono modifiche "pendenti" nel buffer, mandarle al Wand
		if (m_bDataModified) {
			SyncBufferToMagickWand();
			m_bDataModified = FALSE;
		}

		// ora salva il contenuto del Wand (che e' aggiornato)
		return (MagickWriteImage(m_pWand, szPath) == MagickTrue);
	}

	la logica del "traffico":
	Azione, Chi comanda?, Cosa fare?
	GetPixel / Crop,Buffer C++,EnsurePixelBuffer() (Wand -> Buffer)
	SetPixel,Buffer C++,Imposta m_bDataModified = TRUE
	Filtri Magick / Save,ImageMagick,SyncBufferToMagickWand() (Buffer -> Wand)

	creare una piccola funzione InvalidateBuffer()

	void CImageMagick::InvalidateBuffer() {
		if(m_pPixelBuffer) {
			free(m_pPixelBuffer);
			m_pPixelBuffer = NULL;
		}
		m_bufferSize = 0;
		m_bDataModified = FALSE; 
	}

	per quei casi in cui ImageMagick modifica l'immagine (Resize, Rotate).
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include "window.h"
#include "ImageConfig.h"

#ifdef HAVE_IMAGEMAGICK_LIBRARY

#include <stdlib.h>
#include "strings.h"
#include "CImage.h"
#include "CImageMagick.h"

using namespace IM;

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

// inizializza l'unica copia esistente della variabile
// per usare il GetInstanceCount(), toglierla da qui e scommentare le dichiarazioni dentro la classe
LONG s_nInstanceCount = 0;

/*
	CImageMagick()
*/
CImageMagick::CImageMagick()
{
	// descrizione ultimo errore
	memset(m_szError,'\0',sizeof(m_szError));

	// nome file relativo all'immagine
	memset(m_szFileName,'\0',sizeof(m_szFileName));
	memset(m_szFormat,'\0',sizeof(m_szFormat));

	// header metadati
    memset(&m_InfoHeader,'\0',sizeof(IMAGEHEADERINFO));
    m_InfoHeader.type = NULL_PICTURE;
    m_InfoHeader.xres = -1;
    m_InfoHeader.yres = -1;
    m_InfoHeader.restype = -1;
    m_InfoHeader.compression = -1;
    m_InfoHeader.quality = -1;
    m_InfoHeader.filesize = (unsigned long)-1L;
    m_InfoHeader.width = (unsigned long)-1L;
    m_InfoHeader.height = (unsigned long)-1L;
    m_InfoHeader.memused = 0;

	// puntatore all'oggetto immagine
    m_pWand = nullptr;
    m_pPixel = nullptr;

    BITMAPINFO* m_pBMI = nullptr;
    m_pPixelBuffer = nullptr;
    m_bufferSize = 0;
    m_pBMIHeader = nullptr;
    m_BMIHeaderSize = 0;
	m_bDataModified = FALSE;

	// tipi files riconosciuti, notare che ImageMagick supporta circa 200 tipi di formati grafici...
	// qui si includono solo i basici
	// mantenere sincronizzato con la determinazione del tipo immagine in Load()
	LPIMAGETYPE p;
	ADDFILETYPE(AVIF_PICTURE,	".avif","AVIF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".webp","WebP file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)
	ADDFILETYPE(PNG_PICTURE,	".png",	"Portable Network Graphics Format ",IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jpg",	"JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)
	ADDFILETYPE(GIF_PICTURE,	".gif",	"GIF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)

	ADDFILETYPE(BMP_PICTURE,	".bmp",	"BMP file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(BMP_PICTURE,	".dib",	"DIB file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(BMP_PICTURE,	".rle",	"RLE file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(ICO_PICTURE,	".ico",	"Icon file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jpeg","JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jpe",	"JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jif",	"JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jfif","JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(PCX_PICTURE,	".pcx",	"PCX file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(PGM_PICTURE,	".pgm",	"Portable Graymap File ",			IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(PPM_PICTURE,	".ppm",	"PPM file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(TGA_PICTURE,	".tga",	"TARGA file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(TIFF_PICTURE,	".tif",	"TIFF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(TIFF_PICTURE,	".tiff","TIFF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)

	// range e valori di default per i filtri implementati
	SetFilterParams("Blur",				0,50,5,			"","",True,&CImageObject::Blur);
	SetFilterParams("Brightness",		-100,100,50,	"","",True,&CImageObject::Brightness);
	SetFilterParams("MirrorHorizontal",	0,0,0,			"","",True,&CImageObject::MirrorHorizontal);
	SetFilterParams("MirrorVertical",	0,0,0,			"","",True,&CImageObject::MirrorVertical);

	// incrementa il contatore in modo thread-safe
    LONG nCurrent = ::InterlockedIncrement(&s_nInstanceCount);

    // se e' la prima istanza, inizializza ImageMagick
	if(nCurrent==1)
        MagickWandGenesis();

	// verifica i limiti
	MagickSizeType memory_limit, map_limit, disk_limit;
	memory_limit = GetMagickResourceLimit(MemoryResource);
	map_limit = GetMagickResourceLimit(MapResource);
	disk_limit = GetMagickResourceLimit(DiskResource);

/*	printf("ImageMagick limits:\n");
	printf("  Memory: %.2f MB\n", memory_limit / (1024.0*1024.0));
	printf("  Map: %.2f MB\n", map_limit / (1024.0*1024.0));
	printf("  Disk: %.2f MB\n", disk_limit / (1024.0*1024.0));
*/
	// aumenta i limiti
	SetMagickResourceLimit(MemoryResource, 512 * 1024 * 1024);  // 512 MB
	SetMagickResourceLimit(MapResource, 1024 * 1024 * 1024);      // 1 GB

}

/*
	~CImageMagick()
*/
CImageMagick::~CImageMagick()
{
	Unload();
    
    if(m_pPixelBuffer)
	{
        free(m_pPixelBuffer);
        m_pPixelBuffer = nullptr;
        m_bufferSize = 0;
    }
    
    if(m_pBMIHeader)
	{
        free(m_pBMIHeader);
        m_pBMIHeader = nullptr;
        m_BMIHeaderSize = 0;
    }
    
    if(m_pPixel)
	{
        DestroyPixelWand(m_pPixel);
        m_pPixel = nullptr;
    }

	// decrementa il contatore
	LONG nRemaining = ::InterlockedDecrement(&s_nInstanceCount);

    // se e' la ultima istanza, termina ImageMagick
	if(nRemaining==0)
		MagickWandTerminus();
}

/*
	GetWidth()
*/
int CImageMagick::GetWidth(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);
    
	return(MagickGetImageWidth(m_pWand));
}

/*
	GetHeight()
*/
int CImageMagick::GetHeight(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);
    
	return(MagickGetImageHeight(m_pWand));
}

/*
	GetXRes()
*/
float CImageMagick::GetXRes(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);

	double x = 0.0,y = 0.0;
	if(MagickGetImageResolution(m_pWand,&x,&y)==MagickFalse)
        return(0);

    return((float)x);
}

/*
	GetYRes()
*/
float CImageMagick::GetYRes(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);

	double x = 0.0,y = 0.0;
    if(MagickGetImageResolution(m_pWand,&x,&y)==MagickFalse)
		return(0);

    return((float)y);
}

/*
	GetNumColors()
*/
UINT CImageMagick::GetNumColors(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);

	// recupera i bpp truccati, vedi le note in GetBPP(), UpdateHeaderInfo(), etc.
    UINT bpp = GetBPP();
    
	if(bpp==24 || bpp==32)
	{
		return(0);
	}
	else
	{
        // qualcosa e' andato storto con la GetBPP() perche' non ha fatto il trucchetto
		// determina quindi il valore reale, ma occhio perche' tale valore 'reale'
		// fara' sicuramente scasinare il resto del codice...
		
		// per immagini palette
		ImageType type = MagickGetImageType(m_pWand);
		if(type==PaletteType)
		{
			// ImageMagick non ha API diretta per numero colori palette, quindi stima in base al BPP
			if(bpp <= 8)
				return(1 << bpp);
		}
	}
    
    return(0);
}

/*
	GetBytesWidth()
*/
UINT CImageMagick::GetBytesWidth(UINT nWidth,UINT nBpp,UINT nAlig)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);

    // ricava i dati attuali se non riceve parametri
    UINT nFinalWidth = (nWidth == 0) ? (UINT)MagickGetImageWidth(m_pWand) : nWidth;
    UINT nFinalBpp   = (nBpp == 0)   ? (UINT)GetBPP() : nBpp;
    UINT nFinalAlig  = (nAlig == 0)  ? 4 : nAlig;

    // calcolo standard DIB Windows (WIDTHBYTES)
    return(((nFinalWidth * nFinalBpp + (nFinalAlig * 8 - 1)) / (nFinalAlig * 8)) * nFinalAlig);
}

/*
	GetBPP()
*/
UINT CImageMagick::GetBPP(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);

    // se l'immagine ha un canale alpha, usa 32 bit, altrimenti 24
    // questo forza il sistema a ignorare i formati a 1 o 8 bit (grayscale/palette) che causano il problema del fango grigio e del taglio diagonale
	// la classe CImage e le GDI di Windows gestiscono le immagini a 1, 4 o 8 bit tramite la palette dei colori, mentre ImageMagick, quando esporta 
	// in BGR, trasforma tutto in colori reali, quindi se il buffer e' a 8 bit ma non ha una palette corretta nell'header (BITMAPINFO), Windows non 
	// sa come visualizzarlo e "interpreta" i byte a modo suo, creando i problemi di cui sopra
	// al contrario un DIB a 24 o 32 bit non ha bisogno di palette, si mette nel buffer e Windows lo disegna direttamente in modo corretto

    // invece di usare la depth reale (che puo' essere 16), forza a 8 bit per canale per la compatibilita' Windows
    if(MagickGetImageAlphaChannel(m_pWand)==MagickTrue)
        m_InfoHeader.bpp = 32;
    else
        m_InfoHeader.bpp = 24;
    
    // approccio semplice basato sul tipo immagine
    ImageType type = MagickGetImageType(m_pWand);
    switch(type) {
		case BilevelType:			m_InfoHeader.bppOriginal = 1; // 1-bit
        case GrayscaleType:			m_InfoHeader.bppOriginal = 8; // 8-bit grayscale
        case PaletteType:			m_InfoHeader.bppOriginal = 8; // 8-bit indexed
        case GrayscaleAlphaType:	m_InfoHeader.bppOriginal = 16;// 16-bit (8+8)
        case TrueColorType:			m_InfoHeader.bppOriginal = 24;// 24-bit RGB
        case TrueColorAlphaType:	m_InfoHeader.bppOriginal = 32;// 32-bit RGBA
        case ColorSeparationType:	m_InfoHeader.bppOriginal = 32;// CMYK, 4 canali × 8-bit
        default:
			// ricava il valore reale
			size_t depth = MagickGetImageDepth(m_pWand); // bit per canale
            if(MagickGetImageAlphaChannel(m_pWand)==MagickTrue)
                m_InfoHeader.bppOriginal = ((UINT)(depth * 4));	// RGBA
            else
                m_InfoHeader.bppOriginal = ((UINT)(depth * 3));	// RGB
	}

	return(m_InfoHeader.bpp);
}

/*
	GetOriginalBPP()
*/
UINT CImageMagick::GetOriginalBPP(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);

	GetBPP();

	return(m_InfoHeader.bppOriginal);
}

/*
	ConvertToBPP()
*/
UINT CImageMagick::ConvertToBPP(UINT nBitsPerPixel,UINT nFlags,RGBQUAD* pPalette,UINT nColors)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	// forza il tipo (il che rigenera la cache)
	ImageType type;
	switch(nBitsPerPixel) {
		case 8:		type = GrayscaleType;		break;
		case 24:	type = TrueColorType;		break;
		case 32:	type = TrueColorAlphaType;	break;
		default: return(GDI_ERROR);
		}
	MagickSetImageType(m_pWand,type);

	// aggiorna l'header
	UpdateHeaderInfo();

	// svuota il buffer locale, il che obbliga a ri-esportare i pixel dal Wand la prossima volta che verranno chiesti
	if(m_pPixelBuffer)
	{
		free(m_pPixelBuffer);
		m_pPixelBuffer = NULL;
	}

	return(NO_ERROR);
}

/*
	GetPixel()
*/
COLORREF CImageMagick::GetPixel(int x,int y)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);

    // verifica e sincronizza il buffer (una sola volta)
    if(!EnsurePixelBuffer())
		return(0);
    
    // controlla le coordinate
    int width  = MagickGetImageWidth(m_pWand);
    int height = MagickGetImageHeight(m_pWand);
    if(x >= width || y >= height)
        return(0);
    
    UINT bpp = GetBPP();
    size_t channels = (bpp==32) ? 4 : 3;
    size_t stride = GetBytesWidth(width,bpp,4);
    
    // logica top-down
    size_t bufferY = y; 
    
    BYTE* pixel = (BYTE*)m_pPixelBuffer + (bufferY * stride) + (x * channels);
    
    // formato BGR -> RGB per COLORREF
    return(RGB(pixel[2],pixel[1],pixel[0]));
}

/*
	SetPixel()
*/
void CImageMagick::SetPixel(int x,int y,COLORREF colorref)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return;

    // verifica e sincronizza il buffer (una sola volta)
    if(!EnsurePixelBuffer())
		return;
    
    // controlla le coordinate
    int width  = MagickGetImageWidth(m_pWand);
    int height = MagickGetImageHeight(m_pWand);
    if(x >= width || y >= height)
        return;
    
    UINT bpp = GetBPP();
    size_t channels = (bpp==32) ? 4 : 3;
    size_t stride = GetBytesWidth(width,bpp,4);
    
    // logica top-down
    size_t bufferY = y;
    
    BYTE* pixel = (BYTE*)m_pPixelBuffer + (bufferY * stride) + (x * channels);
    
    // scrittura BGR
    pixel[0] = GetBValue(colorref); // Blue
    pixel[1] = GetGValue(colorref); // Green
    pixel[2] = GetRValue(colorref); // Red
    
    if(channels==4)
        pixel[3] = 255; // alpha fisso a opaco per SetPixel standard
    
    // notifica che il buffer e' cambiato rispetto al Wand
    m_bDataModified = TRUE;
}

/*
	GetPixels()
*/
void* CImageMagick::GetPixels(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(nullptr);

	// buffer invalidato
    if(!m_pPixelBuffer)
	{
        SyncMagickWandToBuffer();
        m_bDataModified = FALSE;
    }
    if(!m_pPixelBuffer)
	{
		SetLastErrorDescriptionEx("%s(): invalid data",__func__);
		return(nullptr);
	}

    // controlla le dimensioni
    size_t width = MagickGetImageWidth(m_pWand);
    size_t height = MagickGetImageHeight(m_pWand);
	if(width==0 || height==0)
		return(nullptr);
    
    // determina formato pixel Windows (BGR/BGRA)
    StorageType storageType = CharPixel; // 8-bit per canale
    size_t channels = 3; // RGB
    const char* pixelFormat = "BGR";
    if(MagickGetImageAlphaChannel(m_pWand)==MagickTrue)
	{
        channels = 4; // RGBA
        pixelFormat = "BGRA";
    }
    
    // calcola lo stride (allineato a 4 byte come DIB Windows)
    size_t stride = ((width * channels * sizeof(unsigned char) + 3) / 4) * 4;
    size_t bufferSize = stride * height;
    
    // alloca/rialloca il buffer se necessario
    if(!m_pPixelBuffer || m_bufferSize!=bufferSize)
	{
        if(m_pPixelBuffer)
            free(m_pPixelBuffer);

        m_pPixelBuffer = malloc(bufferSize);
        m_bufferSize = bufferSize;
        
        if(!m_pPixelBuffer)
		{
			SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
            return(nullptr);
		}
    }
    
    // esporta i pixel da ImageMagick in buffer temporaneo
    // ImageMagick esporta top-down, ma necessita bottom-up per DIB Windows
    BYTE* tempBuffer = (BYTE*)malloc(bufferSize);
    if(!tempBuffer)
	{
        free(m_pPixelBuffer);
        m_pPixelBuffer = nullptr;
        m_bufferSize = 0;
		SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
        return(nullptr);
    }
    
	MagickBooleanType status = MagickExportImagePixels(
		m_pWand,
		0,0,			// start X, Y
		width,height,	// region size
		pixelFormat,	// "BGR" o "BGRA" (Windows compatibile)
		storageType,	// CharPixel = unsigned char (8-bit)
		tempBuffer		// buffer temporaneo (top-down)
		);
    
    if(status==MagickFalse)
	{
        // fallback: prova con formato alternativo
        if(channels==4)
		{
            // prova senza alpha
            status = MagickExportImagePixels(m_pWand,0,0,width,height,"BGR",storageType,tempBuffer);
            if(status==MagickTrue)
			{
                channels = 3;
                pixelFormat = "BGR";
                stride = ((width * 3 + 3) / 4) * 4;
                bufferSize = stride * height;
            }
        }
        
        if(status==MagickFalse)
		{
            free(tempBuffer);
            free(m_pPixelBuffer);
            m_pPixelBuffer = nullptr;
            m_bufferSize = 0;
			SetLastErrorDescriptionEx("%s(): invalid data",__func__);
            return(nullptr);
        }
    }
    
	// conversione: top-down (ImageMagick) -> top-down (buffer nostro)
	BYTE* dstBuffer = (BYTE*)m_pPixelBuffer;

	for(size_t y = 0; y < height; y++)
	{
		// sorgente: riga y (top-down)
		BYTE* srcLine = tempBuffer + y * (width * channels);
    
		// destinazione: riga y (mantiene top-down)
		BYTE* dstLine = dstBuffer + (y * stride); 
    
		memcpy(dstLine,srcLine,width * channels);
    
		size_t padding = stride - (width * channels);
		if(padding > 0)
			memset(dstLine + (width * channels),0,padding);
	}

    free(tempBuffer);
    
    return(m_pPixelBuffer);
}

/*
	GetBMI()
*/
LPBITMAPINFO CImageMagick::GetBMI(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(nullptr);
    
    UINT bpp = GetBPP();
    size_t width  = MagickGetImageWidth(m_pWand);
    size_t height = MagickGetImageHeight(m_pWand);
	if(width==0 || height==0 || bpp==0)
		return(nullptr);
    
    // calcola solo per correttezza, perche' con il trucco di GetBPP() ora e' sempre a 0
    size_t paletteSize = 0;
    if(bpp <= 8)
    {
        size_t numColors = 1 << bpp;
        paletteSize = numColors * sizeof(RGBQUAD);
    }
    
    size_t requiredSize = sizeof(BITMAPINFOHEADER) + paletteSize;
    
    if(!m_pBMIHeader || m_BMIHeaderSize < requiredSize)
    {
        if(m_pBMIHeader)
			free(m_pBMIHeader);
        m_pBMIHeader = (BYTE*)malloc(requiredSize);
        m_BMIHeaderSize = requiredSize;
        if(!m_pBMIHeader)
		{
			SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
			return(nullptr);
		}
    }
    
    LPBITMAPINFO pbmi = (LPBITMAPINFO)m_pBMIHeader;
    memset(pbmi, 0, sizeof(BITMAPINFOHEADER));
    
    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pbmi->bmiHeader.biWidth = (LONG)width;

    // usa l'altezza negativa per indicare a Windows che il buffer e' top-down
    pbmi->bmiHeader.biHeight = -(LONG)height; 
    pbmi->bmiHeader.biPlanes = 1;
    pbmi->bmiHeader.biBitCount = (WORD)bpp;
    pbmi->bmiHeader.biCompression = BI_RGB;
    pbmi->bmiHeader.biSizeImage = (DWORD)(WIDTHBYTES(width * bpp, 4) * height);
    pbmi->bmiHeader.biXPelsPerMeter = (LONG)((GetXRes() * 100.0) / 2.54); 
    pbmi->bmiHeader.biYPelsPerMeter = (LONG)((GetYRes() * 100.0) / 2.54);
    
    // calcola solo per correttezza, perche' con il trucco di GetBPP() ora e' sempre 24 o 32
    if(bpp <= 8)
    {
        size_t numColors = 1 << bpp;
        pbmi->bmiHeader.biClrUsed = (DWORD)numColors;
        
        for(size_t i = 0; i < numColors; i++)
        {
            BYTE gray = (BYTE)((i * 255) / ((numColors > 1) ? (numColors - 1) : 1));
            pbmi->bmiColors[i].rgbRed = gray;
            pbmi->bmiColors[i].rgbGreen = gray;
            pbmi->bmiColors[i].rgbBlue = gray;
            pbmi->bmiColors[i].rgbReserved = 0;
        }
    }
    else
    {
        pbmi->bmiHeader.biClrUsed = 0;
    }
    
    return(pbmi);
}

/*
	GetMemUsed()
*/
UINT CImageMagick::GetMemUsed(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);
    
	// calcola la memoria richiesta dal DIB, ImageMagick non ha un metodo al proposito ???

    // usa GetBPP() invece di depth per canale
    UINT bpp = GetBPP();
    size_t width  = MagickGetImageWidth(m_pWand);
    size_t height = MagickGetImageHeight(m_pWand);
    if(width==0 || height==0 || bpp==0)
		return(0);
    
    // calcola stride allineato a 4 byte (come DIB Windows)
    size_t stride = WIDTHBYTES((width * bpp),GetAlignment());

    return((UINT)(stride * height));
}

/*
	GetDIB()
*/
HDIB CImageMagick::GetDIB(UINT* pSize/* = NULL*/)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(NULL);
    
    UINT bpp = GetBPP();
    size_t width  = MagickGetImageWidth(m_pWand);
    size_t height = MagickGetImageHeight(m_pWand);
    if(width == 0 || height == 0 || bpp == 0)
		return NULL;
    
    // calcolo stride e dimensioni
    size_t stride = WIDTHBYTES((width * bpp), GetAlignment());
    size_t imageSize = stride * height;
    
    size_t paletteSize = 0;
    if(bpp <= 8)
        paletteSize = (1 << bpp) * sizeof(RGBQUAD);
    
    size_t totalSize = sizeof(BITMAPINFOHEADER) + paletteSize + imageSize;
    if(totalSize > 0xFFFFFFFFUL) return NULL;
    if(pSize)
		*pSize = (UINT)totalSize;

    // allocazione memoria globale
    HDIB hDib = (HDIB)::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, (SIZE_T)totalSize);
	if(!hDib)
	{
		SetLastErrorDescriptionEx("%s(): DIB allocation failed",__func__);
		return NULL;
	}
    BITMAPINFOHEADER* pDstHeader = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL)hDib);
    if(!pDstHeader) {
        ::GlobalFree((HGLOBAL)hDib);
		SetLastErrorDescriptionEx("%s(): DIB lock failed",__func__);
        return NULL;
    }
    
    // riempimento Header (DIB Standard = altezza POSITIVA = Bottom-Up)
    pDstHeader->biSize			= sizeof(BITMAPINFOHEADER);
    pDstHeader->biWidth			= (LONG)width;
    pDstHeader->biHeight		= (LONG)height; // la lascia positiva per massima compatibilita'
    pDstHeader->biPlanes		= 1;
    pDstHeader->biBitCount		= (WORD)bpp;
    pDstHeader->biCompression	= BI_RGB;
    pDstHeader->biSizeImage		= (DWORD)imageSize;
    pDstHeader->biXPelsPerMeter	= (LONG)((GetXRes() * 100.0) / 2.54);
    pDstHeader->biYPelsPerMeter	= (LONG)((GetYRes() * 100.0) / 2.54);
    
    // gestione palette (se necessaria)
    if(bpp <= 8)
	{
        pDstHeader->biClrUsed = (1 << bpp);
        RGBQUAD* pPalette = (RGBQUAD*)(pDstHeader + 1);
        for(size_t i = 0; i < pDstHeader->biClrUsed; i++)
		{
            BYTE gray = (BYTE)((i * 255) / (pDstHeader->biClrUsed - 1));
            pPalette[i].rgbRed = pPalette[i].rgbGreen = pPalette[i].rgbBlue = gray;
            pPalette[i].rgbReserved = 0;
        }
    }
    
    // copia e ribaltamento pixel
    // pSrc e' Top-Down (buffer interno)
    // pDst deve diventare Bottom-Up (standard HDIB)
    BYTE* pSrcPixels = (BYTE*)GetPixels(); 
    BYTE* pDstPixels = (BYTE*)(pDstHeader + 1) + paletteSize;

    if(!pSrcPixels)
	{
        ::GlobalUnlock((HGLOBAL)hDib);
        ::GlobalFree((HGLOBAL)hDib);
		SetLastErrorDescriptionEx("%s(): invalid DIB data",__func__);
        return NULL;
    }

    for(size_t y = 0; y < height; y++)
	{
        // riga sorgente (dall'alto verso il basso)
        BYTE* pLineSrc = pSrcPixels + (y * stride);
        // riga destinazione (dal basso verso l'alto)
        BYTE* pLineDst = pDstPixels + ((height - 1 - y) * stride);
        
        memcpy(pLineDst, pLineSrc, stride);
    }

    ::GlobalUnlock((HGLOBAL)hDib);

    return(hDib);
}

/*
	SetDIB()
*/
BOOL CImageMagick::SetDIB(HDIB hDib, int nOrientation/* = 1*/)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta impostando ex novo
	//if(!IsValid(__func__))
	//	return(FALSE);

	if(!hDib)
	{
		SetLastErrorDescriptionEx("%s(): invalid DIB data",__func__);
		return(FALSE);
	}

    // blocca la memoria globale
    BITMAPINFOHEADER* pSrcHeader = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL)hDib);
    if(!pSrcHeader)
	{
		SetLastErrorDescriptionEx("%s(): DIB lock failed",__func__);
		return(FALSE);
	}
    
    // validazione
    if( pSrcHeader->biSize < sizeof(BITMAPINFOHEADER) || 
        pSrcHeader->biWidth <= 0 || 
        pSrcHeader->biHeight == 0 ||
        pSrcHeader->biBitCount > 32) 
    {
        ::GlobalUnlock((HGLOBAL)hDib);
        return(FALSE);
    }
    
    // calcolo degli offset
    // la dimensione dell'header puo' essere maggiore di BITMAPINFOHEADER (es. BITMAPV4HEADER)
    size_t headerSize = pSrcHeader->biSize;
    size_t bpp = (size_t)pSrcHeader->biBitCount;
    
    size_t paletteSize = 0;
    if(bpp <= 8)
    {
        // se biClrUsed e' 0 per bpp <= 8, allora la palette e' 2^bpp
        size_t numColors = (pSrcHeader->biClrUsed > 0) ? (size_t)pSrcHeader->biClrUsed : (1 << bpp);
        paletteSize = numColors * sizeof(RGBQUAD);
    }
    
    // i pixel iniziano dopo l'header e la palette
    BYTE* pSrcPixels = (BYTE*)pSrcHeader + headerSize + paletteSize;
    
    // prepara il BITMAPINFO da passare alla Create
    // BITMAPINFO e' essenzialmente un BITMAPINFOHEADER seguito dalla palette
    size_t bmiSize = headerSize + paletteSize;
    BITMAPINFO* pBmi = (BITMAPINFO*)malloc(bmiSize);
    if(!pBmi)
    {
        ::GlobalUnlock((HGLOBAL)hDib);
		SetLastErrorDescriptionEx("%s(): DIB allocation failed",__func__);
        return FALSE;
    }
    
    memcpy(pBmi, pSrcHeader, bmiSize);
    
    // delega alla Create che gestisce internamente:
    // - ribaltamento se biHeight > 0 (Bottom-Up)
    // - rimozione del padding (stride Windows -> stride compatto Magick)
    BOOL bSuccess = Create(pBmi, pSrcPixels);
    
    free(pBmi);
    ::GlobalUnlock((HGLOBAL)hDib);
    
    return bSuccess;
}

/*
	UpdateHeaderInfo()
*/
DWORD CImageMagick::UpdateHeaderInfo(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	m_InfoHeader.width = MagickGetImageWidth(m_pWand);
    m_InfoHeader.height = MagickGetImageHeight(m_pWand);

    m_InfoHeader.bpp = GetBPP();
	// m_InfoHeader.bppOriginal -> impostato dentro GetBPP()
    m_InfoHeader.memused = GetMemUsed();

	m_InfoHeader.xres = GetXRes();
    m_InfoHeader.yres = GetYRes();
    m_InfoHeader.colors = GetNumColors();

    // tipo immagine base
    m_InfoHeader.type = NULL_PICTURE;

	// determina colorSpace in base a come trattiamo l'immagine, non come e' nel Wand
	// ossia in base al trucco di covertire tutto a 24 o 32 bpp che realizza GetBPP()
    if(m_InfoHeader.bpp==32)
        m_InfoHeader.colorSpace = COLOR_RGBA;
    else if(m_InfoHeader.bpp==24)
        m_InfoHeader.colorSpace = COLOR_RGB;
	else
	{
        // qualcosa e' andato storto con la GetBPP() perche' non ha fatto il trucchetto
		// determina quindi il colorSpace reale, ma occhio perche' tale valore 'reale'
		// fara' sicuramente scasinare il resto del codice...
		ImageType type = MagickGetImageType(m_pWand);
		switch(type)
		{
			case BilevelType:
			case GrayscaleType:
				m_InfoHeader.colorSpace = COLOR_GRAYSCALE;
				break;
			case PaletteType:
				m_InfoHeader.colorSpace = COLOR_PALETTE;
				break;
			case TrueColorType:
				m_InfoHeader.colorSpace = COLOR_RGB;
				break;
			case TrueColorAlphaType:
				m_InfoHeader.colorSpace = COLOR_RGBA;
				break;
			default:
				m_InfoHeader.colorSpace = COLOR_UNKNOWN;
		}
	}

	return(NO_ERROR);
}

/*
	CreateImage()
*/
CImage* CImageMagick::CreateImage(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
        return(nullptr);
    
    // crea una nuova istanza
    CImageMagick* pNewImage = new CImageMagick();
    if(!pNewImage)
	{
		SetLastErrorDescriptionEx("%s(): image creation failed",__func__);
        return(nullptr);
    }

    if(pNewImage->m_pWand)
		DestroyMagickWand(pNewImage->m_pWand);
    
    pNewImage->m_pWand = CloneMagickWand(m_pWand);
    if(!pNewImage->m_pWand)
	{
        delete pNewImage;
		SetLastErrorDescriptionEx("%s(): image creation failed",__func__);
        return(nullptr);
    }
    
    // copia i metadati base
    strcpyn(pNewImage->m_szFileName,m_szFileName,sizeof(pNewImage->m_szFileName));
    strcpyn(pNewImage->m_szFormat,m_szFormat,sizeof(pNewImage->m_szFormat));
    memcpy(&pNewImage->m_InfoHeader,&m_InfoHeader,sizeof(IMAGEHEADERINFO));
    
    // i buffer pixel verranno creati on-demand
    pNewImage->m_bDataModified = FALSE;
    
    return(pNewImage);
}

/*
	Create()
*/
BOOL CImageMagick::Create(BITMAPINFO* pBitmapInfo, void* pData)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta creando ex novo
	//if(!IsValid(__func__))
	//	return(FALSE);

    if(!pBitmapInfo || !pData)
	{
		SetLastErrorDescriptionEx("%s(): invalid image data",__func__);
        return(FALSE);
    }

    Unload();

    BITMAPINFOHEADER* pBih = &pBitmapInfo->bmiHeader;
    size_t width = (size_t)pBih->biWidth;
    size_t height = (size_t)abs(pBih->biHeight);
    BOOL isTopDown = (pBih->biHeight < 0);
    size_t bpp = (size_t)pBih->biBitCount;

    if(width==0 || height==0) return FALSE;

    m_pWand = NewMagickWand();
    if(!m_pWand)
	{
		SetLastErrorDescriptionEx("%s(): image creation failed",__func__);
        return(FALSE);
    }

    // decide il formato (normalizza a 24/32 per coerenza)
    // se e' 32 bit usiamo BGRA, altrimenti sempre BGR (anche se l'originale e' 8 bit)
    const char* magickFormat = (bpp == 32) ? "BGRA" : "BGR";
    size_t channels = (bpp == 32) ? 4 : 3;

    // calcolo degli stride
    size_t inputStride = WIDTHBYTES((width * bpp),GetAlignment());	// stride del buffer Windows (con padding)
    size_t compactStride = width * channels;						// stride che vuole ImageMagick (senza padding)

    // crea l'immagine vuota
    PixelWand* background = NewPixelWand();
    PixelSetColor(background, "white");
    MagickNewImage(m_pWand, width, height, background);
    DestroyPixelWand(background);

    // prepariamo un buffer temporaneo "pulito" (senza padding e con orientamento Top-Down)
    BYTE* cleanBuffer = (BYTE*)malloc(compactStride * height);
    if(!cleanBuffer)
	{
        DestroyMagickWand(m_pWand);
        m_pWand = nullptr;
		SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
        return FALSE;
    }

    BYTE* pInputData = (BYTE*)pData;

    for(size_t y = 0; y < height; y++)
	{
        // calcola la riga sorgente gestendo il ribaltamento se Bottom-Up
        size_t srcY = isTopDown ? y : (height - 1 - y);
        BYTE* pSrcLine = pInputData + (srcY * inputStride);
        BYTE* pDstLine = cleanBuffer + (y * compactStride);

        // copia solo i pixel reali, saltando il padding di Windows
        // se bpp e' 8, si dovrebbe fare una conversione a 24bit
        // ma per ora assume che bpp sia coerente con channels
        memcpy(pDstLine, pSrcLine, compactStride);
    }

    // importa finalmente i pixel "puliti"
    MagickBooleanType status = MagickImportImagePixels(
        m_pWand, 0, 0, width, height, magickFormat, CharPixel, cleanBuffer
    );

    free(cleanBuffer);

    if(status == MagickFalse)
	{
        DestroyMagickWand(m_pWand);
        m_pWand = nullptr;
		SetLastErrorDescriptionEx("%s(): invalid image data",__func__);
        return FALSE;
    }

    // forza il tipo TrueColor per evitare che ImageMagick lo declassi a Grayscale
    MagickSetImageType(m_pWand, (channels == 4) ? TrueColorAlphaType : TrueColorType);

    UpdateHeaderInfo();

//$ INSERIRE:
//$   InvalidateBuffer();

    m_bDataModified = FALSE; // il Wand e' appena stato creato dai dati, sono sincronizzati
    
    return TRUE;
}

/*
	Load()
*/
BOOL CImageMagick::Load(LPCSTR lpcszFileName,DWORD& dwError)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta caricando da file
	//if(!IsValid(__func__))
	//	return(FALSE);

	dwError = (DWORD)-1L;

	if(!IsSupportedFormat(lpcszFileName))
		return(FALSE);

    Unload();

    m_pWand = NewMagickWand();
    if(!m_pWand)
		return(FALSE);

	MagickBooleanType status = MagickReadImage(m_pWand,lpcszFileName);
	if(status==MagickFalse)
	{
		ExceptionType severity;
		char *description = MagickGetException(m_pWand,&severity);
		SetLastErrorDescriptionEx("%s(): image loading failed: %s",__func__,description);
		// rilascia la stringa per l'errore
		MagickRelinquishMemory(description);

		return(FALSE);
	}

	// forza il tipo corretto per immagini RGB/RGBA
	ImageType currentType = MagickGetImageType(m_pWand);
	if(currentType==UndefinedType)
	{
		// se tipo indefinito, forza a TrueColor o TrueColorAlpha
		if(MagickGetImageAlphaChannel(m_pWand)==MagickTrue)
			MagickSetImageType(m_pWand,TrueColorAlphaType);
		else
			MagickSetImageType(m_pWand,TrueColorType);
	}

	// salva il nome del file caricato ed imposta il formato in base all'estensione del file
	strcpyn(m_szFileName,lpcszFileName,sizeof(m_szFileName));
	memset(m_szFormat,'\0',sizeof(m_szFormat));
	char* p = strrchr(m_szFileName,'.');
	if(p)
	{
		p++;
		if(p && *p)
		{
			strcpyn(m_szFormat,p,sizeof(m_szFormat));
			strupr(m_szFormat);
		}
	}

	// registra il tipo immagine
	m_InfoHeader.type = NULL_PICTURE;
	if(*m_szFormat)
	{
			if(stricmp(m_szFormat,"BMP")==0 || stricmp(m_szFormat,"DIB")==0 || stricmp(m_szFormat,"RLE")==0)
				m_InfoHeader.type = BMP_PICTURE;
			else if(stricmp(m_szFormat,"GIF")==0)
				m_InfoHeader.type = GIF_PICTURE;
			else if(stricmp(m_szFormat,"ICO") == 0)
				m_InfoHeader.type = ICO_PICTURE;
			else if(stricmp(m_szFormat,"JPG")==0 || stricmp(m_szFormat,"JPEG")==0 || stricmp(m_szFormat,"JPE")==0 || stricmp(m_szFormat,"JIF")==0 || stricmp(m_szFormat,"JFIF")==0)
				m_InfoHeader.type = JPEG_PICTURE;
			else if(stricmp(m_szFormat,"PCX")==0)
				m_InfoHeader.type = PCX_PICTURE;
			else if(stricmp(m_szFormat,"PGM")==0)
				m_InfoHeader.type = PGM_PICTURE;
			else if(stricmp(m_szFormat,"PNG")==0)
				m_InfoHeader.type = PNG_PICTURE;
			else if(stricmp(m_szFormat,"PPM")==0)
				m_InfoHeader.type = PPM_PICTURE;
			else if(stricmp(m_szFormat,"TGA")==0)
				m_InfoHeader.type = TGA_PICTURE;
			else if(stricmp(m_szFormat,"TIFF")==0 || stricmp(m_szFormat,"TIF")==0)
				m_InfoHeader.type = TIFF_PICTURE;
			else if(stricmp(m_szFormat,"AVIF")==0)
				m_InfoHeader.type = AVIF_PICTURE;
			else if(stricmp(m_szFormat,"WEBP")==0)
				m_InfoHeader.type = WEBP_PICTURE;
	}

	// ricava i bpp originali dell'immagine
//	m_InfoHeader.bppOriginal =

	// dimensione file
	HANDLE hFile = ::CreateFile(m_szFileName,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		m_InfoHeader.filesize = ::GetFileSize(hFile,NULL);
		::CloseHandle(hFile);
	}
	else
	{
		dwError = GetLastError();
		return(FALSE);
	}

	// ricava le dimensioni dell'immagine
//	m_InfoHeader.width = 
//	m_InfoHeader.height = 
//	m_InfoHeader.bpp = 
//	m_InfoHeader.memused = 

	// numero di colori nella palette (0 x 32 bpp)
//	m_InfoHeader.colors =

    // risoluzione (DPI)
//	m_InfoHeader.xres =
//    m_InfoHeader.yres =

    // determina colorSpace
//	m_InfoHeader.colorSpace = 

	// compressione e qualita' andrebbero estratte via metadati EXIF/APP via parsing manuale, omette per il momento
	m_InfoHeader.compression = 0;	// nessuna per default
	m_InfoHeader.quality = -1;		// sconosciuta

	dwError = 0L;

	UpdateHeaderInfo();

	return(TRUE);
}

/*
	Unload()
*/
BOOL CImageMagick::Unload()
{
    if(m_pWand)
    {
        // elimina la wand corrente
        DestroyMagickWand(m_pWand);
        m_pWand = nullptr;
        
        // resetta metadati
        memset(m_szFileName,'\0',sizeof(m_szFileName));
        memset(m_szFormat,'\0',sizeof(m_szFormat));
        memset(&m_InfoHeader,'\0',sizeof(m_InfoHeader));
        m_InfoHeader.type = NULL_PICTURE;
        
        // i buffer possono essere riutilizzati, marca come non modificati
        m_bDataModified = FALSE;
        
        return(TRUE);
    }
    return(FALSE);
}

/*
	Save()

	Salva l'immagine corrente con il nome file e nel formato specificati.
	Passare il nome (completo di estensione) con cui salvare il file ed il formato, 
	da indicare in modo diretto (senza il punto, vedi sotto).

	ImageMagick supporta uno sbotto di formati, ma qui i formati supportati sono solo
	quelli definiti all'inizio nel costruttore.
*/
BOOL CImageMagick::Save(LPCSTR lpcszFileName,LPCSTR lpcszFormat,DWORD dwFlags)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	// sincronizza il buffer della classe con il MagickWand
	m_bDataModified = TRUE;

	if(m_bDataModified && m_pPixelBuffer)
	{
        SyncBufferToMagickWand();
        m_bDataModified = FALSE;
    }

    // imposta il formato se specificato
    if(lpcszFormat)
        MagickSetImageFormat(m_pWand,lpcszFormat);

	char szFilename[_MAX_PATH+1] = {0};
	strcpyn(szFilename,lpcszFileName,sizeof(szFilename));

	// gestione specifica per BMP -> forza BI_RGB (senza compressione):
	if(lpcszFormat && stricmp(lpcszFormat,"BMP")==0)
	{
		snprintf(szFilename,sizeof(szFilename),"BMP3:%s",lpcszFileName);

		// disattiva alpha channel per garantire BI_RGB
		MagickSetImageAlphaChannel(m_pWand,DeactivateAlphaChannel);

		// forza tipo TrueColor (24 bit) per garantire non compresso
		MagickSetImageType(m_pWand,TrueColorType);
	}

	// in generale, si usa MagickWriteImage() che ricava automaticamente il formato
	// in cui salvare a seconda della estensione del file:
	// 
	//		MagickWriteImage(magick_wand,"immagine.avif");
	//
	// pero' si possono usare anche le seguenti alternative:
	// 
	//		- salva in formato AVIF anche se il file si chiama "foto.jpg":
	//		  MagickWriteImage(magick_wand,"AVIF:foto.jpg");
	//
	//		- imposta il formato in modo esplicito:
	//		  MagickSetImageFormat(magick_wand,"AVIF");
	//		  MagickWriteImage(magick_wand,"nome_file_senza_estensione");

    // salva su file
	MagickBooleanType status = MagickWriteImage(m_pWand,szFilename);

	if(status!=MagickTrue)
		SetLastErrorDescriptionEx("%s(): unable to save: %s",__func__,szFilename);
	else
		CImage::Flush(lpcszFileName);

    return(status==MagickTrue);
}

/*
	Crop()
*/
CImageObject* CImageMagick::Crop(int x,int y,int width,int height)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

    // raddrizza l'immagine in base ai metadati (risolve il capovolgimento)
    MagickAutoOrientImage(m_pWand);

    // forza il tipo per rigenerare la cache a 8-bit
    MagickSetImageType(m_pWand, (GetBPP() == 32) ? TrueColorAlphaType : TrueColorType);

    // sincronizza il buffer
    if (!SyncMagickWandToBuffer()) 
	{
		SetLastErrorDescriptionEx("%s(): invalid data",__func__);
        return NULL;
	}

    // esegue il Crop della madre
    return CImage::Crop(x, y, width, height);
}

/*
	Stretch()
*/
UINT CImageMagick::Stretch(RECT& rcSize,BOOL bAspectRatio)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);
    
    size_t currentWidth = MagickGetImageWidth(m_pWand);
    size_t currentHeight = MagickGetImageHeight(m_pWand);
    
    int destWidth = rcSize.right - rcSize.left;
    int destHeight = rcSize.bottom - rcSize.top;
    
    int newWidth, newHeight;
    
    if(bAspectRatio)
	{
        float scaleX = (float)destWidth / currentWidth;
        float scaleY = (float)destHeight / currentHeight;
        float scale = (scaleX > scaleY) ? scaleX : scaleY;
        
        newWidth = (int)(currentWidth * scale + 0.5f);
        newHeight = (int)(currentHeight * scale + 0.5f);
    }
	else
	{
        newWidth = destWidth;
        newHeight = destHeight;
    }
    
    if (newWidth <= 0) newWidth = 1;
    if (newHeight <= 0) newHeight = 1;
    
	MagickBooleanType status = MagickResizeImage(
		m_pWand, 
		(size_t)newWidth, 
		(size_t)newHeight, 
		LanczosFilter
		);
    
    if(status==MagickFalse)
	{
		SetLastErrorDescriptionEx("%s(): failed",__func__);
        return(GDI_ERROR);
	}

    // invalida il buffer dopo la modifica
    if(m_pPixelBuffer)
	{
        free(m_pPixelBuffer);
        m_pPixelBuffer = nullptr;
        m_bufferSize = 0;
    }
    m_bDataModified = TRUE;
    
    UpdateHeaderInfo();

    return(NO_ERROR);
}

/*
	Rescale()

	Ridimensiona l'immagine usando l'algoritmo Lanczos3.
	La funzione e' un wrapper per la virtuale della classe madre (CImage), necessario per come funziona
	internamente ImageMagick.

	Quando si devono usare funzioni che accedono direttamente ai pixel, come Crop(), Rescale(), etc., bisogna 
	prima 'movimentare' lo stato interno di ImageMagick, obbligandola ad aggiornare lo stato della pixel cache.
	Se ImageMagick, dopo aver caricato il file, non ha ancora normalizzato internamente i dati, si verifica un
	disallineamento delle righe dei pixel (stride), ed i pixel possono arrivare sfasati, provocando un taglio 
	diagonale nell immagine dopo la manipolazione diretta dei pixel (non sempre, solo con alcune immagini).
*/
CImageObject* CImageMagick::Rescale(int nWidth,int nHeight)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	// ImageMagick, al momento del caricamento, tiene i pixel in uno stato "non-lineare" o con un allineamento 
	// proprietario che la GetPixels non puo' prevedere, quindi la chiamata sotto a una delle due funzioni (le
	// meno costose), obbliga ImageMagick a prendere i pixel grezzi, scompattarli completamente e riorganizzarli 
	// in una griglia lineare e pulita (la pixel cache rigenerata)
	if (!m_bDataModified) 
	{
#if 0
		// ricava il tipo attuale
		ImageType currentType = MagickGetImageType(m_pWand);

		// invece di forzare sempre Alpha, forza la normalizzazione 
		// mantenendo i 24-bit se l'immagine e' TrueColor
		if(currentType==TrueColorType || currentType==TrueColorAlphaType)
		{
			// per restare a 24-bit
			MagickSetImageType(m_pWand,TrueColorType); 
		}
		else 
		{
			// per altri tipi (es. palette), MagickSetImageType la convertira'
			// comunque forzando la rigenerazione della cache
			MagickSetImageType(m_pWand,currentType);
		}
#else
		MagickSetImageType(m_pWand,TrueColorAlphaType);			// migliore
		//MagickTransformImageColorspace(m_pWand,RGBColorspace);// peggiore
#endif

		m_bDataModified = TRUE;

		// reset del buffer locale per forzare GetPixels a ricaricare i dati puliti
		if (m_pPixelBuffer) {
			free(m_pPixelBuffer);
			m_pPixelBuffer = nullptr;
		}
	}

	CImageObject* pRescaled = CImage::Rescale(nWidth,nHeight);
	if(!pRescaled)
		SetLastErrorDescriptionEx("%s(): unable to apply the rescale filter",__func__);
	return(pRescaled);
}

/*
	Blur()
*/
UINT CImageMagick::Blur(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	int nFactor = 0; // <- radius
	double nValue = 0;
	if(GetFilterParams("Blur",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	// calcola sigma (regola empirica)
    double sigma = nFactor / 2.0;

	// parametri:    
	// radius (raggio del kernel di blur: 0.0 - 50.0, dove radius=0 significa "calcola automaticamente da sigma")
	// sigma (deviazione standard della gaussiana: 0.5 - 10.0, controlla quanto "soft" è il blur)

	// esempi:
	// blur leggero
	//MagickBlurImage(wand, 2.0, 1.0);
	// blur medio  
	//MagickBlurImage(wand, 5.0, 2.5);
	// blur pesante
	//MagickBlurImage(wand, 10.0, 5.0);
	// gaussian blur "classico" (sigma = radius/2)
	//MagickBlurImage(wand, radius, radius/2.0);

    MagickBooleanType status = MagickBlurImage(m_pWand,nFactor,sigma);
    
    if(status==MagickFalse)
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the blur filter",__func__);
        return(GDI_ERROR);
    }

    // invalida il buffer dopo la modifica
    if(m_pPixelBuffer)
	{
        free(m_pPixelBuffer);
        m_pPixelBuffer = nullptr;
        m_bufferSize = 0;
    }
    m_bDataModified = TRUE;
    
    return(NO_ERROR);
}

/*
	Brightness()
*/
UINT CImageMagick::Brightness(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	int nFactor = 0;
	double nValue = 0;
	if (GetFilterParams("Brightness", nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	// MagickModulateImage() controlla: brightness, saturation, hue
    // brightness: 100% = nessuna modifica, 200% = doppia luminosità
    double brightnessPercent = 100.0 + nFactor; // -100..100 -> 0..200%
    
    // saturation e hue rimangono invariati (100%)
    MagickBooleanType status = MagickModulateImage(
        m_pWand,
        brightnessPercent,  // brightness (100% = no change)
        100.0,              // saturation (100% = no change)  
        100.0               // hue (100% = no change)
    );
    
    if(status==MagickFalse)
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the brightness filter",__func__);
        return(GDI_ERROR);
    }
    
    // invalida il buffer dopo la modifica
    if(m_pPixelBuffer)
	{
        free(m_pPixelBuffer);
        m_pPixelBuffer = nullptr;
        m_bufferSize = 0;
    }
    m_bDataModified = TRUE;
    
	return(NO_ERROR);
}

/*
	MirrorHorizontal()
*/
UINT CImageMagick::MirrorHorizontal(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);
    
	MagickBooleanType status = MagickFlopImage(m_pWand);
    
    if(status==MagickFalse)
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the mirror horiz. filter",__func__);
        return(GDI_ERROR);
    }
    
    // invalida il buffer dopo la modifica
    if(m_pPixelBuffer)
	{
        free(m_pPixelBuffer);
        m_pPixelBuffer = nullptr;
        m_bufferSize = 0;
    }
    m_bDataModified = TRUE;
    
    UpdateHeaderInfo();
    
    return(NO_ERROR);
}

/*
	MirrorVertical()
*/
UINT CImageMagick::MirrorVertical(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);
    
	MagickBooleanType status = MagickFlipImage(m_pWand);
    
    if(status==MagickFalse)
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the mirror vert. filter",__func__);
        return(GDI_ERROR);
    }
    
    // invalida il buffer dopo la modifica
    if(m_pPixelBuffer)
	{
        free(m_pPixelBuffer);
        m_pPixelBuffer = nullptr;
        m_bufferSize = 0;
    }
    m_bDataModified = TRUE;
    
    UpdateHeaderInfo();
    
    return(NO_ERROR);
}

/*
	EnsurePixelBuffer()
*/
BOOL CImageMagick::EnsurePixelBuffer(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);
    
    size_t width  = MagickGetImageWidth(m_pWand);
    size_t height = MagickGetImageHeight(m_pWand);
    if(width == 0 || height == 0) return FALSE;
    
    // usa le funzioni centralizzate per calcolare lo stride
    // questo garantisce che EnsurePixelBuffer veda la stessa dimensione di SyncMagickWandToBuffer
    UINT bpp = GetBPP(); 
    size_t stride = GetBytesWidth(width, bpp, 4); 
    size_t bufferSize = stride * height;
    
    // sincronizza solo se il buffer e' nullo o la dimensione e' cambiata
    // nota: se e' stata fatta un operazione sul Wand (es. Rotate), bisogna settare m_pPixelBuffer = NULL 
	// o un flag di modifica per forzare questa funzione a ri-sincronizzare
    if(!m_pPixelBuffer || m_bufferSize != bufferSize)
        return(SyncMagickWandToBuffer());
    
    return(m_pPixelBuffer != nullptr);
}

/*
	SyncMagickWandToBuffer()
*/
BOOL CImageMagick::SyncMagickWandToBuffer(void)
{
	/*
	bisogna assicurarsi che ImageMagick converta i suoi dati interni nel formato desiderato prima di copiarli:
	- Grayscale risolto: chiamando MagickSetImageType prima di MagickExportImagePixels, ImageMagick trasforma internamente qualsiasi immagine (anche una GIF in bianco e nero) in un flusso BGR/BGRA a 24/32 bit. Il Crop ricevera' quindi sempre i 3 (o 4) byte per pixel che si aspetta.
	- Taglio Diagonale risolto: separando srcStride (i dati che escono da Magick) da dstStride (lo spazio in memoria Windows), e usando il memset per il padding, garantisce che ogni riga inizi esattamente dove Windows si aspetta. Anche con larghezze "strane" come 1231px, i 3 byte di scarto vengono saltati correttamente.
	*/

	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);
    
    size_t width  = MagickGetImageWidth(m_pWand);
    size_t height = MagickGetImageHeight(m_pWand);
    if(width == 0 || height == 0) return FALSE;
    
    // forza il formato: risolve il problema Grayscale
    // obblighiamo ImageMagick a convertire i pixel interni in RGB o RGBA a 8-bit per canale
    UINT bpp = GetBPP();
    MagickSetImageType(m_pWand, (bpp == 32) ? TrueColorAlphaType : TrueColorType);

    // prepara il buffer
    size_t dstStride = GetBytesWidth(width, bpp, 4);	// allineato a 4 byte (es. 3696)
    size_t srcStride = width * (bpp / 8);				// senza padding (es. 3693)
    size_t totalBufferSize = dstStride * height;

    if(!m_pPixelBuffer || m_bufferSize != totalBufferSize)
	{
        if(m_pPixelBuffer) free(m_pPixelBuffer);
        m_pPixelBuffer = malloc(totalBufferSize);
        m_bufferSize = totalBufferSize;
    }
    if(!m_pPixelBuffer)
	{	
		SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
		return FALSE;
	}

    // esporta in un buffer compatto (senza padding)
    BYTE* tempBuffer = (BYTE*)malloc(srcStride * height);
    if(!tempBuffer)
	{
		SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
		return FALSE;
	}

    const char* format = (bpp == 32) ? "BGRA" : "BGR";
    if(MagickExportImagePixels(m_pWand, 0, 0, width, height, format, CharPixel, tempBuffer) == MagickFalse)
	{
        free(tempBuffer);
		SetLastErrorDescriptionEx("%s(): invalid data",__func__);
        return FALSE;
    }

    // copia con padding: risolve il taglio diagonale
    BYTE* pDst = (BYTE*)m_pPixelBuffer;
    for(size_t y = 0; y < height; y++)
	{
        // riga sorgente (compatta) e destinazione (allineata a 4 byte)
        BYTE* pSrcRow = tempBuffer + (y * srcStride);
        BYTE* pDstRow = pDst + (y * dstStride);
        
        memcpy(pDstRow, pSrcRow, srcStride);
        
        // se c'e' spazio di padding (1, 2 o 3 byte), lo azzera
        if(dstStride > srcStride)
            memset(pDstRow + srcStride, 0, dstStride - srcStride);
    }

    free(tempBuffer);

    return(TRUE);
}

/*
	SyncBufferToMagickWand()
*/
BOOL CImageMagick::SyncBufferToMagickWand(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);
    
    size_t width  = MagickGetImageWidth(m_pWand);
    size_t height = MagickGetImageHeight(m_pWand);
    if(width == 0 || height == 0) return FALSE;

    // determina il formato in base ai BPP attuali del buffer
    size_t bpp = GetBPP(); 
    const char* pixelFormat = (bpp == 32) ? "BGRA" : "BGR";
    size_t channels = (bpp == 32) ? 4 : 3;

    // calcola gli stride
    // strideWin: come sono i dati nel buffer (allineati a 4 byte)
    // strideMagick: come li vuole MagickImportImagePixels (senza padding)
    size_t strideWin    = GetBytesWidth(width, bpp, 4); 
    size_t strideMagick = width * channels; 
    
    // buffer temporaneo per "sgonfiare" i dati dal padding
    BYTE* tempBuffer = (BYTE*)malloc(strideMagick * height);
    if(!tempBuffer)
	{
		SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
		return FALSE;
	}
    
    BYTE* pSrc = (BYTE*)m_pPixelBuffer;
    for(size_t y = 0; y < height; y++)
	{
        // prendiamo la riga dal buffer Windows (allineata)
        BYTE* pSrcRow = pSrc + (y * strideWin);
        // la copiamo nel buffer Magick (compatta)
        BYTE* pDstRow = tempBuffer + (y * strideMagick);
        
        memcpy(pDstRow, pSrcRow, strideMagick);
    }
    
    // importazione nel Wand
    MagickBooleanType status = MagickImportImagePixels(
        m_pWand,
        0, 0, width, height,
        pixelFormat,
        CharPixel,
        tempBuffer
    );
    
    free(tempBuffer);

    // sincronizzazione finale:
    // ora che i pixel sono nel Wand, forzia il tipo interno
    // e' vitale se l'immagine era nata Grayscale ma ora e' BGR
    if(status == MagickTrue)
	{
        MagickSetImageType(m_pWand, (bpp == 32) ? TrueColorAlphaType : TrueColorType);
        return TRUE;
    }
    
	SetLastErrorDescriptionEx("%s(): invalid data",__func__);

    return(FALSE);
}

/*
	IsValid()
*/
BOOL CImageMagick::IsValid(LPCSTR lpcszFunctionName)
{
	if(!m_pWand)
	{
		char buffer[128] = {0};
		snprintf(buffer,sizeof(buffer),"%s(): no image loaded",lpcszFunctionName);
		CImage::SetLastErrorDescription(buffer);
		#ifdef DEBUG
			::MessageBox(NULL,GetLastErrorDescription(),GetLibraryName(),MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
		#endif
	}
	return(m_pWand!=NULL);
}

#endif
