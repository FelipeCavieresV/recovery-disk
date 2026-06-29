#include "SignatureDatabase.h"

// Solo firmas con headers de al menos 4 bytes o con subHeader para evitar
// falsos positivos. Prioridad: formatos comunes en USB/cámaras/teléfonos.

QVector<FileSignature> SignatureDatabase::loadDefaultSignatures()
{
    QVector<FileSignature> signatures;

    // ── IMÁGENES ──────────────────────────────────────────────────────────

    // JPEG — 3 variantes de cabecera (JFIF, Exif, raw)
    for (const auto& hdr : {
        QByteArray::fromHex("FFD8FFE0"),
        QByteArray::fromHex("FFD8FFE1"),
        QByteArray::fromHex("FFD8FFDB"),
        QByteArray::fromHex("FFD8FFE8"),
    }) {
        FileSignature sig;
        sig.extension = "jpg";
        sig.header    = hdr;
        sig.footer    = QByteArray::fromHex("FFD9");
        sig.hasFooter = true;
        sig.maxSizeMb = 50;
        signatures.push_back(sig);
    }

    // PNG
    {
        FileSignature sig;
        sig.extension = "png";
        sig.header    = QByteArray::fromHex("89504E470D0A1A0A");
        sig.footer    = QByteArray::fromHex("49454E44AE426082");
        sig.hasFooter = true;
        sig.maxSizeMb = 100;
        signatures.push_back(sig);
    }

    // GIF
    for (const auto& hdr : { QByteArray("GIF87a"), QByteArray("GIF89a") }) {
        FileSignature sig;
        sig.extension = "gif";
        sig.header    = hdr;
        sig.footer    = QByteArray::fromHex("003B");
        sig.hasFooter = true;
        sig.maxSizeMb = 30;
        signatures.push_back(sig);
    }

    // TIFF — little endian y big endian (8 bytes, muy específico)
    {
        FileSignature sig;
        sig.extension = "tiff";
        sig.header    = QByteArray::fromHex("49492A00");
        sig.hasFooter = false;
        sig.maxSizeMb = 200;
        signatures.push_back(sig);
    }
    {
        FileSignature sig;
        sig.extension = "tiff";
        sig.header    = QByteArray::fromHex("4D4D002A");
        sig.hasFooter = false;
        sig.maxSizeMb = 200;
        signatures.push_back(sig);
    }

    // Photoshop PSD
    {
        FileSignature sig;
        sig.extension = "psd";
        sig.header    = QByteArray::fromHex("38425053");  // 8BPS
        sig.hasFooter = false;
        sig.maxSizeMb = 500;
        signatures.push_back(sig);
    }

    // HEIC/HEIF (iPhone)
    {
        FileSignature sig;
        sig.extension  = "heic";
        sig.header     = QByteArray::fromHex("00000018");
        sig.subHeader  = QByteArray("ftypheic");
        sig.subHeaderOffset = 0;
        sig.hasFooter  = false;
        sig.maxSizeMb  = 50;
        signatures.push_back(sig);
    }
    {
        FileSignature sig;
        sig.extension  = "heic";
        sig.header     = QByteArray::fromHex("0000001C");
        sig.subHeader  = QByteArray("ftypheix");
        sig.subHeaderOffset = 0;
        sig.hasFooter  = false;
        sig.maxSizeMb  = 50;
        signatures.push_back(sig);
    }

    // ── VIDEOS ────────────────────────────────────────────────────────────

    // MP4 — ftyp box. El size varía pero ftyp siempre está en bytes 4-7.
    // Usamos subHeader para verificar "ftyp" en offset 4.
    for (const auto& size : {
        QByteArray::fromHex("00000018"),
        QByteArray::fromHex("00000020"),
        QByteArray::fromHex("00000014"),
        QByteArray::fromHex("0000001C"),
        QByteArray::fromHex("00000024"),
    }) {
        FileSignature sig;
        sig.extension       = "mp4";
        sig.header          = size;
        sig.subHeader       = QByteArray("ftyp");
        sig.subHeaderOffset = 4;
        sig.hasFooter       = false;
        sig.maxSizeMb       = 4096;
        signatures.push_back(sig);
    }

    // MOV (QuickTime) — ftyp qt
    {
        FileSignature sig;
        sig.extension       = "mov";
        sig.header          = QByteArray::fromHex("00000014");
        sig.subHeader       = QByteArray("ftypqt");
        sig.subHeaderOffset = 4;
        sig.hasFooter       = false;
        sig.maxSizeMb       = 4096;
        signatures.push_back(sig);
    }

    // AVI — RIFF + "AVI " en offset 8
    {
        FileSignature sig;
        sig.extension       = "avi";
        sig.header          = QByteArray("RIFF");
        sig.subHeader       = QByteArray("AVI ");
        sig.subHeaderOffset = 8;
        sig.hasFooter       = false;
        sig.maxSizeMb       = 4096;
        signatures.push_back(sig);
    }

    // MKV / WEBM
    {
        FileSignature sig;
        sig.extension = "mkv";
        sig.header    = QByteArray::fromHex("1A45DFA3");
        sig.hasFooter = false;
        sig.maxSizeMb = 4096;
        signatures.push_back(sig);
    }

    // WMV / ASF
    {
        FileSignature sig;
        sig.extension = "wmv";
        sig.header    = QByteArray::fromHex("3026B2758E66CF11");
        sig.hasFooter = false;
        sig.maxSizeMb = 4096;
        signatures.push_back(sig);
    }

    // FLV
    {
        FileSignature sig;
        sig.extension = "flv";
        sig.header    = QByteArray::fromHex("464C560105");  // FLV + version + flags
        sig.hasFooter = false;
        sig.maxSizeMb = 2048;
        signatures.push_back(sig);
    }

    // MPEG-PS / VOB
    {
        FileSignature sig;
        sig.extension = "mpg";
        sig.header    = QByteArray::fromHex("000001BA");
        sig.hasFooter = false;
        sig.maxSizeMb = 4096;
        signatures.push_back(sig);
    }

    // 3GP
    {
        FileSignature sig;
        sig.extension       = "3gp";
        sig.header          = QByteArray::fromHex("00000014");
        sig.subHeader       = QByteArray("ftyp3gp");
        sig.subHeaderOffset = 4;
        sig.hasFooter       = false;
        sig.maxSizeMb       = 500;
        signatures.push_back(sig);
    }

    // ── AUDIO ─────────────────────────────────────────────────────────────

    // MP3 con ID3 tag (larga y específica)
    {
        FileSignature sig;
        sig.extension = "mp3";
        sig.header    = QByteArray::fromHex("494433");  // ID3
        sig.hasFooter = false;
        sig.maxSizeMb = 100;
        signatures.push_back(sig);
    }

    // WAV — RIFF + "WAVE" en offset 8
    {
        FileSignature sig;
        sig.extension       = "wav";
        sig.header          = QByteArray("RIFF");
        sig.subHeader       = QByteArray("WAVE");
        sig.subHeaderOffset = 8;
        sig.hasFooter       = false;
        sig.maxSizeMb       = 500;
        signatures.push_back(sig);
    }

    // FLAC
    {
        FileSignature sig;
        sig.extension = "flac";
        sig.header    = QByteArray::fromHex("664C6143");  // fLaC
        sig.hasFooter = false;
        sig.maxSizeMb = 500;
        signatures.push_back(sig);
    }

    // OGG
    {
        FileSignature sig;
        sig.extension = "ogg";
        sig.header    = QByteArray::fromHex("4F676753");  // OggS
        sig.hasFooter = false;
        sig.maxSizeMb = 200;
        signatures.push_back(sig);
    }

    // M4A
    {
        FileSignature sig;
        sig.extension       = "m4a";
        sig.header          = QByteArray::fromHex("00000020");
        sig.subHeader       = QByteArray("ftypM4A ");
        sig.subHeaderOffset = 4;
        sig.hasFooter       = false;
        sig.maxSizeMb       = 200;
        signatures.push_back(sig);
    }

    // WMA — mismo magic que WMV
    {
        FileSignature sig;
        sig.extension = "wma";
        sig.header    = QByteArray::fromHex("3026B2758E66CF11");
        sig.hasFooter = false;
        sig.maxSizeMb = 200;
        signatures.push_back(sig);
    }

    // ── DOCUMENTOS ────────────────────────────────────────────────────────

    // PDF
    {
        FileSignature sig;
        sig.extension = "pdf";
        sig.header    = QByteArray("%PDF-");
        sig.footer    = QByteArray("%%EOF");
        sig.hasFooter = true;
        sig.maxSizeMb = 500;
        signatures.push_back(sig);
    }

    // Office 97-2003 (doc, xls, ppt) — Compound Document
    {
        FileSignature sig;
        sig.extension = "doc";
        sig.header    = QByteArray::fromHex("D0CF11E0A1B11AE1");
        sig.hasFooter = false;
        sig.maxSizeMb = 100;
        signatures.push_back(sig);
    }
    {
        FileSignature sig;
        sig.extension = "xls";
        sig.header    = QByteArray::fromHex("D0CF11E0A1B11AE1");
        sig.hasFooter = false;
        sig.maxSizeMb = 100;
        signatures.push_back(sig);
    }

    // Office 2007+ Open XML (ZIP-based, PK\x03\x04 + byte 6-7 = 14 00 o 00 00)
    {
        FileSignature sig;
        sig.extension = "docx";
        sig.header    = QByteArray::fromHex("504B03041400");
        sig.hasFooter = false;
        sig.maxSizeMb = 200;
        signatures.push_back(sig);
    }

    // ZIP genérico
    {
        FileSignature sig;
        sig.extension = "zip";
        sig.header    = QByteArray::fromHex("504B0304");
        sig.hasFooter = false;
        sig.maxSizeMb = 2000;
        signatures.push_back(sig);
    }

    // RTF
    {
        FileSignature sig;
        sig.extension = "rtf";
        sig.header    = QByteArray("{\\rtf1");
        sig.hasFooter = false;
        sig.maxSizeMb = 50;
        signatures.push_back(sig);
    }

    // ── COMPRIMIDOS ───────────────────────────────────────────────────────

    // RAR4
    {
        FileSignature sig;
        sig.extension = "rar";
        sig.header    = QByteArray::fromHex("526172211A0700");
        sig.hasFooter = false;
        sig.maxSizeMb = 4000;
        signatures.push_back(sig);
    }
    // RAR5
    {
        FileSignature sig;
        sig.extension = "rar";
        sig.header    = QByteArray::fromHex("526172211A070100");
        sig.hasFooter = false;
        sig.maxSizeMb = 4000;
        signatures.push_back(sig);
    }
    // 7-Zip
    {
        FileSignature sig;
        sig.extension = "7z";
        sig.header    = QByteArray::fromHex("377ABCAF271C");
        sig.hasFooter = false;
        sig.maxSizeMb = 4000;
        signatures.push_back(sig);
    }

    // SQLite
    {
        FileSignature sig;
        sig.extension = "sqlite";
        sig.header    = QByteArray("SQLite format 3\000", 16);
        sig.hasFooter = false;
        sig.maxSizeMb = 500;
        signatures.push_back(sig);
    }

    return signatures;
}
