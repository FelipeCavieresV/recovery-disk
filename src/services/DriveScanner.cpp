#include "DriveScanner.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QList<DriveInfo> DriveScanner::getAvailableDrives()
{
    QList<DriveInfo> drives;

#ifdef Q_OS_WIN
    DWORD driveMask = GetLogicalDrives();

    for (char letter = 'A'; letter <= 'Z'; ++letter)
    {
        if (!(driveMask & (1 << (letter - 'A'))))
            continue;

        QString rootPath = QString("%1:/").arg(letter);

        wchar_t volumeName[MAX_PATH + 1] = {0};
        wchar_t fileSystemName[MAX_PATH + 1] = {0};

        DWORD serialNumber = 0;
        DWORD maxComponentLength = 0;
        DWORD fileSystemFlags = 0;

        BOOL volumeOk = GetVolumeInformationW(
            reinterpret_cast<LPCWSTR>(rootPath.utf16()),
            volumeName,
            MAX_PATH,
            &serialNumber,
            &maxComponentLength,
            &fileSystemFlags,
            fileSystemName,
            MAX_PATH
        );

        ULARGE_INTEGER freeBytesAvailable;
        ULARGE_INTEGER totalBytes;
        ULARGE_INTEGER totalFreeBytes;

        BOOL spaceOk = GetDiskFreeSpaceExW(
            reinterpret_cast<LPCWSTR>(rootPath.utf16()),
            &freeBytesAvailable,
            &totalBytes,
            &totalFreeBytes
        );

        DriveInfo info;
        info.path = rootPath;
        info.name = QString("%1:").arg(letter);
        info.fileSystem = volumeOk ? QString::fromWCharArray(fileSystemName) : "Desconocido";
        info.totalBytes = spaceOk ? static_cast<qint64>(totalBytes.QuadPart) : 0;
        info.freeBytes = spaceOk ? static_cast<qint64>(totalFreeBytes.QuadPart) : 0;
        info.isReady = volumeOk && spaceOk;

        drives.append(info);
    }
#endif

    return drives;
}

QString DriveScanner::formatBytes(qint64 bytes)
{
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    const double tb = gb * 1024.0;

    if (bytes >= tb)
        return QString::number(bytes / tb, 'f', 2) + " TB";

    if (bytes >= gb)
        return QString::number(bytes / gb, 'f', 2) + " GB";

    if (bytes >= mb)
        return QString::number(bytes / mb, 'f', 2) + " MB";

    if (bytes >= kb)
        return QString::number(bytes / kb, 'f', 2) + " KB";

    return QString::number(bytes) + " B";
}