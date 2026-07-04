/*$
	inet.c
	Internet et varia.
	Luca Piergentili, Luglio '25
*/

/*
Note sui formati grafici:
Common File			Format Type (Full Name),					Description
Extension(s),
---------------------------------------------------------------------------
.jpg,.jpeg			JPEG (Joint Photographic Experts Group)		The most widespread format for digital photography. It uses a "lossy" compression method, balancing file size reduction with minimal perceptible quality loss. Ideal for complex images with smooth color gradients.
.png				PNG (Portable Network Graphics)				A lossless compression format that supports transparency (alpha channel) and millions of colors. Excellent for web graphics, icons, logos, screenshots, and images containing text or sharp lines.
.gif				GIF (Graphics Interchange Format)			Supports simple animations and 1-bit transparency. Its color palette is limited to 256 colors. Best for small icons, simple graphics, and short, looping animations (like memes).
.webp				WebP										A modern format developed by Google, offering superior compression (both lossy and lossless) compared to JPEG and PNG, while also supporting transparency and animation. Highly recommended for web optimization.
.svg				SVG (Scalable Vector Graphics)				An XML-based vector image format. SVG images can be scaled to any size without losing quality, making them ideal for logos, icons, charts, and illustrations that need to adapt to various resolutions.
.tif,.tiff			TIFF (Tagged Image File Format)				A high-quality, often lossless format widely used in professional photography, printing, and graphic design. It supports various color spaces and different compression methods (including lossless ones).
.bmp				BMP (Bitmap)								An uncompressed bitmap image format, resulting in very large file sizes. It offers high quality but is less efficient for web use. Commonly associated with Windows systems.
.heif,.heic			HEIF (High Efficiency Image File Format)	A high-efficiency format primarily used by Apple devices (iPhone/iPad). It offers better compression than JPEG while maintaining high quality, though broader compatibility may require conversion.
.avif				AVIF (AV1 Image File Format)				A newer open-source format based on the AV1 video codec. It boasts superior compression to WebP and HEIF with excellent quality, supporting HDR, transparency, and animation. Its adoption is growing.
.ico				ICO (Windows Icon file)						The format for Windows icons, frequently used for website "favicons." It can contain multiple image sizes and color depths within a single file.
.psd				PSD (Photoshop Document)					Adobe Photoshop's native format. It retains layers, effects, text, and other editable properties, making it essential for image editing and complex design workflows. Not used directly on the web.
.ai					AI (Adobe Illustrator Artwork)				Adobe Illustrator's native vector format. Used for creating and editing high-quality vector graphics for print and professional design. Not used directly on the web.
.cr2				RAW (Camera Raw Image File)					Uncompressed or minimally compressed files directly from a camera's sensor. They contain maximum image data, providing the most flexibility for professional post-processing. Not typically for direct distribution.
.nef				RAW (Camera Raw Image File)					Uncompressed or minimally compressed files directly from a camera's sensor. They contain maximum image data, providing the most flexibility for professional post-processing. Not typically for direct distribution.
.arw				RAW (Camera Raw Image File)					Uncompressed or minimally compressed files directly from a camera's sensor. They contain maximum image data, providing the most flexibility for professional post-processing. Not typically for direct distribution.
.apng				APNG (Animated Portable Network Graphics)	An extension of PNG that supports animation, similar to GIF but with superior quality and full alpha transparency support. Browser compatibility is improving.
.jxl				JXL (JPEG XL)								A new universal image format designed to be more efficient than JPEG, PNG, and GIF. It supports both lossy and lossless compression, transparency, animation, and HDR. Still in the early stages of adoption.
.eps				EPS (Encapsulated PostScript)				A vector format (can also contain raster data) widely used for print graphics. It allows for excellent scalability and is supported by many graphic design software applications.
.mp4				MPEG-4 Part 14								Il formato video più diffuso e compatibile. Ideale per lo streaming web, supportato da quasi tutti i browser e dispositivi. Utilizza codec come H.264 (AVC) per video e AAC per audio, offrendo un buon equilibrio tra qualità e dimensione del file. Supporta anche codec più moderni ed efficienti come H.265 (HEVC) e AV1 per una migliore compressione.
.webm				WebM										Un formato open-source e royalty-free sviluppato da Google. È ottimizzato per il web, offrendo un'eccellente compressione con i codec video VP8 e VP9, e audio con Opus o Vorbis. È molto efficiente per lo streaming e ampiamente supportato dai browser moderni (Chrome, Firefox, Edge, Opera). Rappresenta un'ottima alternativa a MP4, specialmente per chi cerca soluzioni open.
.mov				QuickTime File Format						Sviluppato da Apple, è un formato comune nell'editing video professionale e negli ecosistemi Apple. Può contenere vari codec video (incluso H.264) e audio. Sebbene sia supportato da molti lettori, per la riproduzione diretta sul web è meno universale di MP4 e WebM, e potrebbe richiedere codec specifici o essere transcodificato per un'ampia compatibilità browser.
.mkv				Matroska Video (MKV)						Un container multimediale open-source estremamente flessibile e potente. Può contenere un numero illimitato di tracce video, audio, sottotitoli e metadati in un unico file. È molto popolare per la distribuzione di film e serie TV di alta qualità (spesso "rip" di Blu-ray/DVD) e per l'archiviazione, grazie alla sua capacità di supportare una vasta gamma di codec (inclusi quelli più recenti e avanzati come H.265/HEVC e AV1) senza perdita di qualità.
.avi				Audio Video Interleave (AVI)				Uno dei formati più vecchi e ampiamente riconosciuti, sviluppato da Microsoft nel 1992. È un formato container che può contenere video e audio con una grande varietà di codec.
.wmv				Windows Media Video (WMV)					Un formato sviluppato da Microsoft, primariamente per Windows Media Player e per lo streaming su internet in ambienti Microsoft. WMV è sia un codec che un formato container.
.flv				Flash Video (FLV)							Era il formato standard per la maggior parte dei contenuti video su internet (incluso YouTube) prima dell'avvento dell'HTML5 <video> e del declino di Adobe Flash Player.
.mpeg,.mpg			MPEG-1 / MPEG-2								Questi sono standard più antichi per la compressione video.MPEG-1** è stato il pioniere per i video su CD e per il video su internet a bassa risoluzione. * **MPEG-2** è il formato standard per i **DVD** e per molte trasmissioni televisive digitali (DVB, ATSC). Sebbene non sia più il formato preferito per lo streaming web moderno (a favore di H.264/H.265/AV1), è ancora estremamente rilevante per la compatibilità con i media fisici e i sistemi di trasmissione broadcast.
.avchd,.mts,.m2ts	Advanced Video Coding High Definition		Un formato basato su file per la registrazione video ad alta definizione, sviluppato congiuntamente da Sony e Panasonic. Usa la compressione H.264/MPEG-4 AVC. È comune nelle videocamere digitali HD e per la masterizzazione su DVD e Blu-ray.

Codec: il container (il formato del file, es. .mp4) è come la scatola, mentre il codec (es. H.264, VP9, AV1) è il metodo di compressione usato per il video e l'audio all'interno di quella scatola. L'efficienza e la qualità del video sono determinate principalmente dal codec. I codec più recenti come AV1 (che può stare sia in .mp4 che in .webm) offrono una compressione significativamente migliore, ma la loro compatibilità hardware/software è ancora in crescita.
*/ 
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include <stdbool.h>
#include "url.h"
#include "inet.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/* OCCHIO alla distinzione: array per la lista dei protocolli internet ("://") */
static const char* _internetprotocols[] = {
        "http://",
		"https://",
		"ftp://",
		"ftps://",
		"file://",
        "telnet://",
		"ssh://",
		"sftp://",
		"irc://",
		"gopher://",
        "ws://",
		"wss://",
		"smd://",
		"z39.50r://",
		"imap://",
		"imaps://",
        "pop://",
		"pops://",
		"ldap://",
		"ldaps://",
		"nntp://",
		"rtsp://",
		NULL
    };

/* array per la lista dei protocolli internet NON autoritativi (":") */
static const char* _internetnonauthprotocols[] = {
		"mailto:",
		"news:",
        "sip:",
		"sips:",
		"sms:",
		"urn:",
		"uuid:",
		"xmpp:",
		"view-source:",
		NULL
    };

/*
	Note:

	Un file con estensione .php non ha un Content-Type specifico perche' non e' un file statico da servire, ma uno script che viene eseguito dal server.
	Il server non invia il codice sorgente PHP al client, esegue invece il codice ed invia al client il risultato di tale esecuzione.
	Il Content-Type che si riceva dipende quindi da cosa lo script PHP produce:
	- text/html se lo script genera una pagina web
	- text/plain o application/json se genera dei dati per un'applicazione
	- image/jpeg o image/png se genera dinamicamente un'immagine
	Questo a dimostrazione del fatto che nome del file nella url (in questo caso .php) non ha nessuna correlazione con il Content-Type: inviato dal server.
	L'unica informazione attendibile e' sempre e solo l'header Content-Type: della risposta, in definitiva, l'estensione dipende dalla Content-Type:, e non
	viceversa, e' sempre la Content-Type: la che guida.

	Esistono molte altre estensioni che, per ragioni simili a .php o per altri motivi, non hanno un tipo MIME fisso e standardizzato:

	- File Eseguibili dal Server
	Il motivo principale e' che l'estensione non identifica un file statico, ma uno script che il server deve eseguire. Il server restituisce l'output dello 
	script, ed il tipo di questo output puo' variare. Le estensioni piu' comuni in questa categoria sono:
	.asp: (Active Server Pages) di Microsoft
	.jsp: (JavaServer Pages)
	.cgi o .pl: (Perl)
	.py: (Python)
	In tutti questi casi, il server non serve il file di testo contenente il codice sorgente, ma il risultato dell'esecuzione di quel codice, che potrebbe 
	essere HTML, JSON, un'immagine, o qualsiasi altro tipo di contenuto.

	- File Generici o Interni
	Ci sono anche estensioni che non hanno un tipo MIME associato perche' identificano file che non sono pensati per essere serviti su una rete, o il cui 
	contenuto e' variabile per natura. Esempi includono:
	.dat o .bin: File di dati binari generici. Il server probabilmente restituirebbe application/octet-stream se richiesto, ma non c'e' un tipo specifico.
	.tmp: File temporanei. Il loro contenuto non e' standardizzato.
	.conf o .ini: File di configurazione. Anche se il loro contenuto e' testo, non sono pensati per essere serviti via HTTP.
*/

/* "Los 40" dei tipi MIME, ordinati per Content-Type */
static const MIMETYPE _mime_types[] = {
/*genre			family              (content)type						file ext		description*/

{GENRE_TEXT,	FAMILY_APPLICATION,	2,	"application/atom+xml",				".atom",		"Atom Syndication Format, an XML-based web content syndication format."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/gzip",					".gz",			"Gzip compressed data, often used for web content compression."},
{GENRE_TEXT,	FAMILY_TEXT,		5,	"application/javascript",			".js",			"JavaScript code, essential for interactive web pages."},
{GENRE_TEXT,	FAMILY_TEXT,		5,	"application/json",					".json",		"JavaScript Object Notation, widely used for data interchange in APIs."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/msword",				".doc",			"Legacy Microsoft Word documents, pre-2007 binary format."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/octet-stream",			".bin",			"Generic binary data; used when the file type is unknown or arbitrary."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/pdf",					".pdf",			"Portable Document Format documents, widely used for static documents."},
{GENRE_TEXT,	FAMILY_TEXT,		2,	"application/rss+xml",				".rss",			"RSS (Really Simple Syndication) Feed, for distributing web content."},
{GENRE_TEXT,	FAMILY_APPLICATION,	2,	"application/rtf",					".rtf",			"Rich Text Format, universal document interchange format with formatting."},
{GENRE_TEXT,	FAMILY_APPLICATION,	2,	"application/sql",					".sql",			"SQL database scripts, structured query language for database operations."},
{GENRE_TEXT,	FAMILY_TEXT,		2,	"application/vnd.google-earth.kml+xml",".kml",		"KML (Keyhole Markup Language), an XML-based format for geographic data."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/vnd.ms-excel",			".xls",			"Legacy Microsoft Excel spreadsheets, pre-2007 binary format."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/vnd.ms-powerpoint",	".ppt",			"Legacy Microsoft PowerPoint presentations, pre-2007 binary format."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",".xlsx","Microsoft Excel XLSX documents (modern format)."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/vnd.openxmlformats-officedocument.presentationml.presentation",".pptx","Microsoft PowerPoint PPTX documents (modern format)."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/vnd.openxmlformats-officedocument.wordprocessingml.document",".docx","Microsoft Word DOCX documents (modern format)."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/wasm",					".wasm",		"WebAssembly modules, a binary instruction format for web browsers."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/x-7z-compressed",		".7z",			"7-Zip compressed archives."},
{GENRE_TEXT,	FAMILY_APPLICATION,	2,	"application/x-httpd-php",			".php",			"PHP source code files, server-side scripting language for web development."},
{GENRE_TEXT,	FAMILY_APPLICATION,	2,	"application/x-python-code",		".py",			"Python source code files, widely used general-purpose programming language."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/x-rar-compressed",		".rar",			"RAR compressed archives."},
{GENRE_BINARY,	FAMILY_APPLICATION,	2,	"application/x-shockwave-flash",	".swf",			"Adobe Flash content, historically common for rich media (now largely deprecated)."},
{GENRE_TEXT,	FAMILY_APPLICATION,	2,	"application/x-www-form-urlencoded","N/A",			"Data submitted from HTML forms, URL-encoded for transmission."},
{GENRE_TEXT,	FAMILY_HTML,		2,	"application/xhtml+xml",			".xhtml",		"XHTML documents, a stricter XML-based variant of HTML."},
{GENRE_TEXT,	FAMILY_HTML,		2,	"application/xhtml+xml",			".xhtm",		"XHTML documents, a stricter XML-based variant of HTML."},
{GENRE_TEXT,	FAMILY_TEXT,		5,	"application/xml",					".xml",			"XML data, a general-purpose markup language for structured data."},
{GENRE_BINARY,	FAMILY_APPLICATION,	5,	"application/zip",					".zip",			"ZIP compressed archives, common for bundling multiple files."},

{GENRE_BINARY,	FAMILY_AUDIO,		2,	"audio/aac",						".aac",			"Advanced Audio Coding, common in MPEG-4 containers and modern streaming."},
{GENRE_BINARY,	FAMILY_AUDIO,		2,	"audio/flac",						".flac",		"Free Lossless Audio Codec, popular for high-fidelity audio archiving."},
{GENRE_BINARY,	FAMILY_AUDIO,		2,	"audio/mpeg",						".mp3",			"MPEG audio files (e.g., MP3), a very common audio compression format."},
{GENRE_BINARY,	FAMILY_AUDIO,		2,	"audio/ogg",						".ogg",			"Ogg Vorbis audio files, an open-source alternative to MP3."},
{GENRE_BINARY,	FAMILY_AUDIO,		2,	"audio/ogg",						".oga",			"Ogg Vorbis audio files, an open-source alternative to MP3."},
{GENRE_BINARY,	FAMILY_AUDIO,		2,	"audio/wav",						".wav",			"WAV (Waveform Audio File Format), an uncompressed audio format."},
{GENRE_BINARY,	FAMILY_AUDIO,		2,	"audio/x-ms-wma",					".wma",			"Windows Media Audio, legacy Microsoft compressed audio format."},

{GENRE_BINARY,	FAMILY_FONT,		2,	"font/otf",							".otf",			"OpenType Font files."},
{GENRE_BINARY,	FAMILY_FONT,		2,	"font/ttf",							".ttf",			"TrueType Font files."},
{GENRE_BINARY,	FAMILY_FONT,		2,	"font/woff2",						".woff2",		"WOFF2 (Web Open Font Format 2), modern and efficient web font format."},

{GENRE_BINARY,	FAMILY_IMAGE,		6,	"image/avif",						".avif",		"AV1 Image File Format, modern highly efficient image codec from Alliance for Open Media."},
{GENRE_BINARY,	FAMILY_IMAGE,		6,	"image/gif",						".gif",			"GIF images, commonly used for animated graphics and simple icons."},
{GENRE_BINARY,	FAMILY_IMAGE,		6,	"image/heic",						".heic",		"HEIF/HEIC images, Apple's efficient image format for iOS device photos."},
{GENRE_BINARY,	FAMILY_IMAGE,		6,	"image/heif",						".heif",		"HEIF images, High Efficiency Image File Format baseline container."},
{GENRE_BINARY,	FAMILY_IMAGE,		6,	"image/jpeg",						".jpeg",		"JPEG images, a highly common lossy image format for photos."},
{GENRE_BINARY,	FAMILY_IMAGE,		6,	"image/jpeg",						".jpg",			"JPEG images, a highly common lossy image format for photos."},
{GENRE_BINARY,	FAMILY_IMAGE,		6,	"image/jpeg",						".jfif",		"JPEG images, a highly common lossy image format for photos."},
{GENRE_BINARY,	FAMILY_IMAGE,		6,	"image/jxl",						".jxl",			"JPEG XL, next-generation image format superior to JPEG, PNG, and GIF."},
{GENRE_BINARY,	FAMILY_IMAGE,		5,	"image/png",						".png",			"PNG images, a common lossless image format, supports transparency."},
{GENRE_TEXT,	FAMILY_IMAGE,		2,	"image/svg+xml",					".svg",			"SVG (Scalable Vector Graphics), XML-based vector images for the web."},
{GENRE_BINARY,	FAMILY_IMAGE,		2,	"image/webp",						".webp",		"WebP images, a modern image format offering superior compression."},
{GENRE_BINARY,	FAMILY_IMAGE,		2,	"image/x-icon",						".ico",			"ICO files, typically used for favicons on websites."},

{GENRE_BINARY,	FAMILY_TEXT,		2,	"multipart/form-data",				"N/A",			"HTML form data that includes file uploads or multiple parts."},

{GENRE_TEXT,	FAMILY_TEXT,		5,	"text/css",							".css",			"CSS stylesheets, crucial for web page styling."},
{GENRE_TEXT,	FAMILY_TEXT,		2,	"text/csv",							".csv",			"CSV (Comma-Separated Values) files, commonly used for tabular data exchange."},
{GENRE_TEXT,	FAMILY_TEXT,		2,	"text/javascript",					".js",			"Older/alternative type for JavaScript, still sometimes seen."},
{GENRE_TEXT,	FAMILY_TEXT,		2,	"text/javascript",					".mjs",			"JavaScript ES6 module."},
{GENRE_TEXT,	FAMILY_HTML,		6,	"text/html",						".html",		"HTML documents (web pages), fundamental for the web."},
{GENRE_TEXT,	FAMILY_HTML,		6,	"text/html",						".htm",			"HTML documents (web pages), fundamental for the web."},
{GENRE_TEXT,	FAMILY_TEXT,		2,	"text/markdown",					".md",			"Markdown text files, lightweight markup language extremely common in development."},
{GENRE_TEXT,	FAMILY_TEXT,		6,	"text/plain",						".txt",			"Simple, unformatted text files."},

{GENRE_BINARY,	FAMILY_VIDEO,		2,	"video/avi",						".avi",			"Audio Video Interleave, legacy but still common Windows container format."},
{GENRE_BINARY,	FAMILY_VIDEO,		2,	"video/ogg",						".ogv",			"Ogg Theora video files, an open-source video compression format."},
{GENRE_BINARY,	FAMILY_VIDEO,		2,	"video/mp4",						".mp4",			"MP4 video files, a widely supported video and audio container format."},
{GENRE_BINARY,	FAMILY_VIDEO,		2,	"video/quicktime",					".mov",			"QuickTime video format, extremely common from Apple devices and cameras."},
{GENRE_BINARY,	FAMILY_VIDEO,		2,	"video/webm",						".webm",		"WebM video files, an open-source, royalty-free media file format."},
{GENRE_BINARY,	FAMILY_VIDEO,		2,	"video/x-flv",						".flv",			"Flash Video, legacy streaming format still encountered in older content."},
{GENRE_BINARY,	FAMILY_VIDEO,		2,	"video/x-matroska",					".mkv",			"Matroska video container, popular open-source format for high-quality media."},
{GENRE_RESERVED,FAMILY_RESERVED,	0,	"",									"",				""}
};

/* puntatore d'appoggio per poter far puntare l'array a quello del chiamante invece che a quello di cui sopra, vedi inet_load_mime_types()  piu' sotto */
static const MIMETYPE* _mimetypes = &_mime_types[0];

/*
definisce gli array di puntatori delle famiglie per il web content
ognuno di questi array viene rappresentato nella struttura WEBCONTENTFAMILYTYPE, dove family rappresenta
l'array (della famiglia) e type i puntatori (ossia gli elementi) di ognuno di questi array
la funzione inet_enum_web_content_formats() si incarica di passare tutti i puntatori (elementi) contenuti nell'array 
della famiglia specificata dalla macro ENUM_...
*/

/* #define ENUM_GRAPHICS_FORMAT 0 */
static const char* _graphics_format_array[] = {				
	".ai",
	".apng",
	".arw",
	".avchd",
	".avi",
	".avif",
	".bmp",
	".cr2",
	".eps",
	".flv",
	".gif",
	".heic",
	".heif",
	".ico",
	".jfif",
	".jpeg",
	".jpg",
	".jxl",
	".m2ts",
	".mkv",
	".mov",
	".mp4",
	".mpeg",
	".mpg",
	".mts",
	".nef",
	".png",
	".psd",
	".svg",
	".tif",
	".tiff",
	".webm",
	".webp",
	".wmv",
	NULL
    };

bool inet_is_graphics_format(const char* filename)
{
    bool itis = false;

    for(int i=0; _graphics_format_array[i]!=NULL; i++)
        if(striright(filename,_graphics_format_array[i])==0)
        {
            itis = true;
            break;
        }

    return(itis);
}

/* #define ENUM_SCRIPT_TYPE 1 */
static const char* _script_type_array[] = {		
	".js",
	".json",
	".mjs",
	".wasm",
	NULL
    };

bool inet_is_script_format(const char* filename)
{
    bool itis = false;

    for(int i=0; _script_type_array[i]!=NULL; i++)
        if(striright(filename,_script_type_array[i])==0)
        {
            itis = true;
            break;
        }

    return(itis);
}

/* #define ENUM_STYLE_SHEET 2 */
static const char* _style_sheet_array[] = {	
	".css",
	".otf",
	".ttf",
	".woff",
	".woff2",
	NULL
    };

bool inet_is_style_format(const char* filename)
{
    bool itis = false;

    if(striright(filename,_style_sheet_array[0])==0)
        itis = true;

    return(itis);
}

bool inet_is_font_format(const char* filename)
{
    bool itis = false;

    for(int i=1; _script_type_array[i]!=NULL; i++)
        if(striright(filename,_script_type_array[i])==0)
        {
            itis = true;
            break;
        }

    return(itis);
}

/* #define ENUM_AUDIO_FORMAT 3 */
static const char* _audio_format_array[] = {	
	".flac",
	".mp3",
	".ogg",
	".wav",
	NULL
    };

bool inet_is_audio_format(const char* filename)
{
    bool itis = false;

    for(int i=0; _audio_format_array[i]!=NULL; i++)
        if(striright(filename,_audio_format_array[i])==0)
        {
            itis = true;
            break;
        }

    return(itis);
}

/* #define ENUM_DATA_FORMAT 4 */
static const char* _data_format_array[] = {	
	".csv",
	".json",
	".xml",
	NULL
    };

bool inet_is_data_format(const char* filename)
{
    bool itis = false;

    for(int i=0; _data_format_array[i]!=NULL; i++)
        if(striright(filename,_data_format_array[i])==0)
        {
            itis = true;
            break;
        }

    return(itis);
}

/* #define ENUM_DOCUMENTS 5 */
static const char* _docs_array[] = {	
	".csv",
	".doc",
	".docx",
	".odp",
	".ods",
	".odt",
	".pdf",
	".ppt",
	".pptx",
	".txt",
	".xls",
	".xlsx",
	NULL
    };

bool inet_is_doc_format(const char* filename)
{
    bool itis = false;

    for(int i=0; _docs_array[i]!=NULL; i++)
        if(striright(filename,_docs_array[i])==0)
        {
            itis = true;
            break;
        }

    return(itis);
}

/* #define ENUM_COMPRESS_FORMAT 6 */
static const char* _compress_format_array[] = {	
	".7z",
	".ace",
	".arc",
	".arj",
	".cab",
	".gz",
	".iso",
	".lha",
	".lzh",
	".rar",
	".tar",
	".Z",
	".zip",
	".zoo",
	NULL
    };

bool inet_is_compress_format(const char* filename)
{
    bool itis = false;

    for(int i=0; _compress_format_array[i]!=NULL; i++)
        if(striright(filename,_compress_format_array[i])==0)
        {
            itis = true;
            break;
        }

    return(itis);
}

/*
	inet_enum_internet_protocols()
*/
//$ distinguere due funzioni differentei per i due array
const char* inet_enum_internet_protocols(int* iterator)
{
    while(_internetprotocols[(*iterator)]!=NULL)
		return(_internetprotocols[(*iterator)++]);
	return(NULL);
}

/*
	inet_enum_mime_types()
*/
const MIMETYPE* inet_enum_mime_types(int* iterator)
{
    while(_mimetypes[(*iterator)].genre!=GENRE_RESERVED)
		return(&_mimetypes[(*iterator)++]);
	return(NULL);
}

/*
	inet_load_mime_types()
*/
void inet_load_mime_types(const MIMETYPE* mt)
{
	/* sostituisce l'array interno de "Los 40" cone quello del chiamante */
	_mimetypes = mt;
}

/*
	inet_enum_graphics_formats()
*/
const char* inet_enum_graphics_formats(int* iterator)
{
    while(_graphics_format_array[*iterator]!=NULL)
		return(_graphics_format_array[(*iterator)++]);
	return(NULL);
}

/*
	inet_enum_web_content_formats()

	Enumera (lista) gli elementi (tipi di files) di uno degli array (famiglie) di cui sopra.
	Il concetto di base e' definire il web content come formato da famiglie di tipi (di files).
*/
const char* inet_enum_web_content_formats(int* iterator,int type)
{
	/* array di arrays (vedi sopra) */
	static const char* const* array_of_arrays[] = {
		_graphics_format_array,
		_script_type_array,
		_style_sheet_array,
		_audio_format_array,
		_data_format_array,
		_docs_array,
		_compress_format_array,
		NULL
	};
	
	/* controlla che l'indice per selezionare l'array ricevuto come parametro stia nel range */
	int bound = (sizeof(array_of_arrays) / sizeof(array_of_arrays[0])) - 1;
	if(type >= 0 && type < bound)
	{
		/* seleziona l'array (in base all'indice ricevuto come parametro) e ne elenca il contenuto */
		const char* const* selected_array = array_of_arrays[type];
		while(selected_array[*iterator]!=NULL)
			return(selected_array[(*iterator)++]);
	}
	
	return(NULL);
}
