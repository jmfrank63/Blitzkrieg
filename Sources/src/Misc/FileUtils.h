#ifndef __FILE_UTILS_H__
#define __FILE_UTILS_H__
#pragma ONCE








#if defined(_WIN32) || defined(_WIN64)

#ifdef GetFileTitle
#undef GetFileTitle
#endif
#endif
#if !defined(_WIN32) && !defined(_WIN64)
#define BLITZKRIEG_FILETIME_DEFINED
struct FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; };
#endif

namespace NFile
{
class CFile
{
    std::FILE *hFile;
    std::string szFilePath;
public:
    struct SStatus {
        FILETIME ctime, mtime, atime;
        int nSize;
        DWORD dwAttributes;
        std::string szPathName;
    };
    enum EOpenFlags {
        modeRead = 0x0000, modeWrite = 0x0001, modeReadWrite = 0x0002,
        shareCompat = 0x0000, shareExclusive = 0x0010, shareDenyWrite = 0x0020,
        shareDenyRead = 0x0030, shareDenyNone = 0x0040, modeNoInherit = 0x0080,
        modeCreate = 0x1000, modeNoTruncate = 0x2000
    };
    enum EAttribute {
        readOnly = 0x00000001, hidden = 0x00000002, system = 0x00000004,
        directory = 0x00000010, archive = 0x00000020, encrypted = 0x00000040,
        normal = 0x00000080, temporary = 0x00000100, sparse_file = 0x00000200,
        reparse_point = 0x00000400, compressed = 0x00000800, offline = 0x00001000,
        not_content_indexed = 0x00002000
    };
    enum ESeekPosition { begin = 0x0, current = 0x1, end = 0x2 };
    CFile() : hFile(nullptr) {}
    CFile(const char *name, DWORD flags) : hFile(nullptr) { Open(name, flags); }
    virtual ~CFile() { Close(); }
    bool Open(const char *name, DWORD flags);
    CFile *Duplicate() const;
    void Close();
    bool Flush();
    bool IsOpen() const { return hFile != nullptr; }
    int Read(void *buffer, int count);
    int Write(const void *buffer, int count);
    int Seek(int offset, ESeekPosition from);
    int GetPosition() const;
    int SetLength(int length);
    int GetLength() const;
    bool GetStatus(SStatus *status) const;
    DWORD GetAttributes() const;
    bool SetAttributes(DWORD attributes);
    bool SetFileTime(const FILETIME *, const FILETIME *, const FILETIME *);
    operator std::FILE *() const { return hFile; }
    const std::string &GetFilePath() const;
    const std::string GetFileName() const;
    const std::string GetFileTitle() const;
    const std::string GetFileExt() const;
    static bool Rename(const char *, const char *);
    static bool Remove(const char *);
    static DWORD GetAttributes(const char *);
    static bool SetAttributes(const char *, DWORD);
    static bool SetFileTime(const char *, const FILETIME *, const FILETIME *, const FILETIME *);
};

class CFileIterator
{
    struct Entry { std::string name; std::string path; DWORD attributes; FILETIME ctime, atime, mtime; int size; };
    std::vector<Entry> entries;
    std::size_t index = 0;
    std::string szPath;
    std::string szMask;
    const Entry *Current() const { return index < entries.size() ? &entries[index] : nullptr; }
public:
    CFileIterator() = default;
    explicit CFileIterator(const char *mask) { FindFirstFile(mask); }
    ~CFileIterator() { Close(); }
    const CFileIterator &FindFirstFile(const char *mask);
    const CFileIterator &Next();
    bool Close();
    bool IsEnd() const { return Current() == nullptr; }
    const CFileIterator &operator++() { return Next(); }
    DWORD GetAttribs() const { return Current() ? Current()->attributes : 0; }
    bool IsReadOnly() const { return (GetAttribs() & CFile::readOnly) != 0; }
    bool IsSystem() const { return (GetAttribs() & CFile::system) != 0; }
    bool IsHidden() const { return (GetAttribs() & CFile::hidden) != 0; }
    bool IsTemporary() const { return (GetAttribs() & CFile::temporary) != 0; }
    bool IsNormal() const { return (GetAttribs() & CFile::normal) != 0; }
    bool IsArchive() const { return (GetAttribs() & CFile::archive) != 0; }
    bool IsCompressed() const { return (GetAttribs() & CFile::compressed) != 0; }
    bool IsDirectory() const { return (GetAttribs() & CFile::directory) != 0; }
    bool IsDots() const { const std::string n = GetFileName(); return IsDirectory() && (n == "." || n == ".."); }
    bool IsMatchesMask(DWORD mask) const { return (GetAttribs() & mask) == mask; }
    FILETIME GetCreationTime() const { return Current() ? Current()->ctime : FILETIME{}; }
    FILETIME GetLastAccessTime() const { return Current() ? Current()->atime : FILETIME{}; }
    FILETIME GetLastWriteTime() const { return Current() ? Current()->mtime : FILETIME{}; }
    int GetLength() const { return Current() ? Current()->size : 0; }
    const std::string GetFileName() const { return Current() ? Current()->name : std::string(); }
    const std::string GetFilePath() const { return Current() ? Current()->path : std::string(); }
    const std::string GetFileTitle() const;
    const std::string GetFileExt() const;
    const std::string &GetBasePath() const { return szPath; }
    const std::string &GetBaseMask() const { return szMask; }
};

template <class TEnumFunc>
void EnumerateFiles(const char *start, const char *mask, TEnumFunc callback, bool recurse)
{
    std::string directory = start ? start : "";
    if (!directory.empty() && directory.back() != '\\' && directory.back() != '/') directory += '\\';
    for (CFileIterator it((directory + mask).c_str()); !it.IsEnd(); ++it) if (!it.IsDirectory()) callback(it);
    if (!recurse) return;
    for (CFileIterator it((directory + "*").c_str()); !it.IsEnd(); ++it) {
        if (it.IsDirectory() && !it.IsDots()) {
            const std::string child = it.GetFilePath() + "\\";
            EnumerateFiles(child.c_str(), mask, callback, true);
            callback(it);
        }
    }
}

class CGetAllFiles { std::vector<std::string> *files; public: explicit CGetAllFiles(std::vector<std::string> *v) : files(v) {} void operator()(const CFileIterator &it) { if (!it.IsDirectory()) files->push_back(it.GetFilePath()); } };
class CGetAllFilesRelative { std::vector<std::string> *files; std::string initial; public: CGetAllFilesRelative(const char *d, std::vector<std::string> *v) : files(v), initial(d) {} void operator()(const CFileIterator &it) { if (!it.IsDirectory()) { std::string n = it.GetFilePath(); if (n.size() > initial.size()) files->push_back(n.substr(initial.size())); } } };
class CGetAllDirectoriesRelative { std::vector<std::string> *files; std::string initial; public: CGetAllDirectoriesRelative(const char *d, std::vector<std::string> *v) : files(v), initial(d) {} void operator()(const CFileIterator &it) { if (!it.IsDirectory()) { std::string n = it.GetFilePath(); if (n.size() <= initial.size()) return; n = n.substr(initial.size()); const std::size_t slash = n.rfind('\\'); if (slash == std::string::npos) return; n.resize(slash); if (!n.empty() && std::find(files->begin(), files->end(), n) == files->end()) files->push_back(n); } } };
void DeleteFiles(const char *, const char *, bool);
void DeleteDirectory(const char *);
void CreatePath(const char *);
void GetDirNames(const char *, std::list<std::string> *, bool = true);
void GetFileNames(const char *, const char *, std::list<std::string> *, bool = true);
bool IsFileExist(const char *);
std::string GetFullName(const std::string &);
double GetFreeDiskSpace(const char *);
}

template <class TYPE> inline NFile::CFile &operator<<(NFile::CFile &file, const TYPE &data) { file.Write(&data, sizeof(data)); return file; }
template <class TYPE> inline NFile::CFile &operator>>(NFile::CFile &file, TYPE &data) { file.Read(&data, sizeof(data)); return file; }
template <> inline NFile::CFile &operator<<(NFile::CFile &file, const std::string &data) { int n = static_cast<int>(data.size()); file << n; file.Write(data.data(), n); return file; }
template <> inline NFile::CFile &operator>>(NFile::CFile &file, std::string &data) { int n = 0; file >> n; data.resize(n); if (n > 0) file.Read(&data[0], n); return file; }
template <> inline NFile::CFile &operator<<(NFile::CFile &file, const std::wstring &data) { int n = static_cast<int>(data.size()); file << n; file.Write(data.data(), n * sizeof(wchar_t)); return file; }
template <> inline NFile::CFile &operator>>(NFile::CFile &file, std::wstring &data) { int n = 0; file >> n; data.resize(n); if (n > 0) file.Read(&data[0], n * sizeof(wchar_t)); return file; }
#endif
