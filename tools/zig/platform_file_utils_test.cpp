#include "Misc/FileUtils.h"
#include <cassert>
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
