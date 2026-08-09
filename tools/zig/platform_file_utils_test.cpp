// FileUtils.h first: it pulls in LegacyTypes.h, which is what fixes FILETIME's
// layout for everyone. The game's StdAfx.h headers do the same.
#include "Misc/FileUtils.h"
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include "Platform/PortableCrt.h"
#endif
#include <cassert>
#include <ctime>
#include <filesystem>
#include <fstream>

int main()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "blitzkrieg-file-utils-test";
    std::filesystem::remove_all(root);
    NFile::CreatePath((root / "nested").string().c_str());
    const std::string file_name = (root / "nested" / "value.txt").string();
    NFile::CFile file(file_name.c_str(), NFile::CFile::modeCreate | NFile::CFile::modeReadWrite);
    assert(file.IsOpen());
    const char value[] = "portable";
    assert(file.Write(value, 8) == 8);
    assert(file.Flush());
    assert(file.GetPosition() == 8);
    assert(file.Seek(0, NFile::CFile::begin) == 0);
    char readback[9] = {};
    assert(file.Read(readback, 8) == 8);
    assert(std::string(readback) == "portable");
    assert(file.SetLength(4) == 4);
    assert(file.GetLength() == 4);
    NFile::CFile *duplicate = file.Duplicate();
    assert(duplicate != nullptr);
    delete duplicate;
    file.Close();

    std::list<std::string> names;
    NFile::GetFileNames((root / "nested").string().c_str(), "*.txt", &names, true);
    assert(names.size() == 1);
    NFile::CFileIterator iterator((root / "nested" / "*.txt").string().c_str());
    assert(!iterator.IsEnd() && iterator.GetLength() == 4 && !iterator.IsDirectory());
    assert(iterator.GetFileExt() == "txt");
    assert(iterator.GetFileTitle() == "value");
    assert(!iterator.IsEnd());
    ++iterator;
    assert(iterator.IsEnd());

    // The save game list builds its date through GetStatus ->
    // FileTimeToLocalFileTime -> FileTimeToSystemTime. While a FILETIME held
    // raw seconds none of that decoded, and every save showed 00.00.0000 00:00.
    {
        NFile::CFile stamped(file_name.c_str(), NFile::CFile::modeRead);
        assert(stamped.IsOpen());
        NFile::CFile::SStatus status;
        assert(stamped.GetStatus(&status));
        stamped.Close();
        const unsigned long long ticks = (static_cast<unsigned long long>(status.mtime.dwHighDateTime) << 32) | status.mtime.dwLowDateTime;
        const long long stamped_unix = static_cast<long long>(ticks / 10000000ULL) - 11644473600LL;
        const long long now = static_cast<long long>(std::time(nullptr));
        assert(stamped_unix > now - 600 && stamped_unix < now + 600);

        FILETIME local = {};
        assert(FileTimeToLocalFileTime(&status.mtime, &local) != 0);
        SYSTEMTIME broken = {};
        assert(FileTimeToSystemTime(&local, &broken) != 0);

        const std::time_t stamped_time = static_cast<std::time_t>(stamped_unix);
        std::tm expected = {};
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&expected, &stamped_time);
#else
        localtime_r(&stamped_time, &expected);
#endif
        assert(broken.wYear == expected.tm_year + 1900);
        assert(broken.wMonth == expected.tm_mon + 1);
        assert(broken.wDay == expected.tm_mday);
        assert(broken.wHour == expected.tm_hour);
        assert(broken.wMinute == expected.tm_min);
    }

    const std::string renamed = (root / "nested" / "renamed.bin").string();
    assert(NFile::CFile::Rename(file_name.c_str(), renamed.c_str()));
    assert(NFile::IsFileExist(renamed.c_str()));
    assert(NFile::CFile::Remove(renamed.c_str()));
    assert(!NFile::IsFileExist(renamed.c_str()));
    assert(NFile::GetFreeDiskSpace(root.string().c_str()) > 0.0);
    NFile::DeleteDirectory(root.string().c_str());
    assert(!std::filesystem::exists(root));
    return 0;
}
