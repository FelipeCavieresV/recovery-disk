#include "SignatureDatabase.h"

QVector<FileSignature> SignatureDatabase::loadDefaultSignatures()
{
    QVector<FileSignature> signatures;

    {
        FileSignature jpg;

        jpg.extension = "jpg";
        jpg.header = QByteArray::fromHex("FFD8FF");
        jpg.footer = QByteArray::fromHex("FFD9");
        jpg.hasFooter = true;
        jpg.maxSizeMb = 100;

        signatures.push_back(jpg);
    }

    {
        FileSignature png;

        png.extension = "png";
        png.header = QByteArray::fromHex("89504E470D0A1A0A");
        png.footer = QByteArray::fromHex("49454E44AE426082");
        png.hasFooter = true;
        png.maxSizeMb = 50;

        signatures.push_back(png);
    }

    {
        FileSignature pdf;

        pdf.extension = "pdf";
        pdf.header = QByteArray("%PDF");
        pdf.footer = QByteArray("%%EOF");
        pdf.hasFooter = true;
        pdf.maxSizeMb = 200;

        signatures.push_back(pdf);
    }

    {
        FileSignature zip;

        zip.extension = "zip";
        zip.header = QByteArray::fromHex("504B0304");
        zip.footer.clear();
        zip.hasFooter = false;
        zip.maxSizeMb = 500;

        signatures.push_back(zip);
    }

    {
        FileSignature docx;

        docx.extension = "docx";
        docx.header = QByteArray::fromHex("504B0304");
        docx.footer.clear();
        docx.hasFooter = false;
        docx.maxSizeMb = 500;

        signatures.push_back(docx);
    }

    {
        FileSignature xlsx;

        xlsx.extension = "xlsx";
        xlsx.header = QByteArray::fromHex("504B0304");
        xlsx.footer.clear();
        xlsx.hasFooter = false;
        xlsx.maxSizeMb = 500;

        signatures.push_back(xlsx);
    }

    {
        FileSignature pptx;

        pptx.extension = "pptx";
        pptx.header = QByteArray::fromHex("504B0304");
        pptx.footer.clear();
        pptx.hasFooter = false;
        pptx.maxSizeMb = 500;

        signatures.push_back(pptx);
    }

    {
        FileSignature mp4;

        mp4.extension = "mp4";
        mp4.header = QByteArray("ftyp");
        mp4.footer.clear();
        mp4.hasFooter = false;
        mp4.maxSizeMb = 2048;

        signatures.push_back(mp4);
    }

    return signatures;
}