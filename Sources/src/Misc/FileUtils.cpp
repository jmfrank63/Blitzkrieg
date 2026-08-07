#include "StdAfx.h"
#include "FileUtils.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <cstdint>
#include <limits>
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#else
#include <io.h>
#endif

namespace {
namespace fs = std::filesystem;

std::string nativePath(std::string value) {
    for (char &c : value) if (c == '\\') c = fs::path::preferred_separator;
    return value;
}
std::string legacyPath(std::string value) {
#if defined(_WIN32) || defined(_WIN64)
    for (char &c : value) if (c == '/') c = '\\';
#else
    for (char &c : value) if (c == '\\') c = '/';
#endif
    return value;
}
bool wildcard(const std::string &pattern, const std::string &name) {
    std::size_t p = 0, n = 0, star = std::string::npos, star_n = 0;
    while (n < name.size()) {
        if (p < pattern.size() && pattern[p] != '*' && pattern[p] != '?' &&
            (pattern[p] == name[n] || std::tolower(static_cast<unsigned char>(pattern[p])) == std::tolower(static_cast<unsigned char>(name[n])))) { ++p; ++n; }
        else if (p < pattern.size() && pattern[p] == '*') { star = p++; star_n = n; }
        else if (p < pattern.size() && pattern[p] == '?') { ++p; ++n; }
        else if (star != std::string::npos) { p = star + 1; n = ++star_n; }
        else return false;
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}
FILETIME fileTime(const fs::path &path) {
    FILETIME result{};
    std::error_code error;
    const auto value = fs::last_write_time(path, error);
    if (error) return result;
    const auto ticks = std::chrono::duration_cast<std::chrono::seconds>(value.time_since_epoch()).count();
    const std::uint64_t raw = ticks < 0 ? 0 : static_cast<std::uint64_t>(ticks);
    result.dwLowDateTime = static_cast<DWORD>(raw & 0xffffffffu);
    result.dwHighDateTime = static_cast<DWORD>(raw >> 32);
    return result;
}
DWORD attributes(const fs::directory_entry &entry) {
    DWORD result = 0;
    std::error_code error;
    if (entry.is_directory(error)) result |= NFile::CFile::directory;
    else result |= NFile::CFile::normal;
    const std::string name = entry.path().filename().string();
    if (!name.empty() && name[0] == '.') result |= NFile::CFile::hidden;
    const auto perms = entry.status(error).permissions();
    if ((perms & fs::perms::owner_write) == fs::perms::none) result |= NFile::CFile::readOnly;
    return result;
}
std::string splitDirectory(const std::string &mask, std::string &file_mask) {
    const std::string native = nativePath(mask);
    const std::size_t slash = native.find_last_of("/\\");
    if (slash == std::string::npos) { file_mask = native; return "."; }
    file_mask = native.substr(slash + 1);
    return native.substr(0, slash).empty() ? "." : native.substr(0, slash);
}
}

namespace NFile {
bool CFile::Open(const char *name, DWORD flags) {
    if (!name) return false;
    const DWORD mode = flags & 3;
    const bool create = (flags & modeCreate) != 0;
    const bool no_truncate = (flags & modeNoTruncate) != 0;
    const char *open_mode = mode == modeRead ? "rb" : mode == modeWrite ? (create && !no_truncate ? "wb" : "rb+") : (create && !no_truncate ? "wb+" : "rb+");
    const std::string full = GetFullName(name);
    hFile = std::fopen(nativePath(full).c_str(), open_mode);
    if (!hFile && create && no_truncate) hFile = std::fopen(nativePath(full).c_str(), mode == modeRead ? "rb" : "ab+");
    if (!hFile) return false;
    szFilePath = full;
    return true;
}
CFile *CFile::Duplicate() const {
    if (!IsOpen()) return nullptr;
    CFile *copy = new CFile();
    if (!copy->Open(szFilePath.c_str(), modeReadWrite | modeNoTruncate)) { delete copy; return nullptr; }
    copy->Seek(GetPosition(), begin);
    return copy;
}
void CFile::Close() { if (hFile) std::fclose(hFile); hFile = nullptr; }
bool CFile::Flush() { return IsOpen() && std::fflush(hFile) == 0; }
int CFile::Read(void *buffer, int count) { return IsOpen() && buffer && count > 0 ? static_cast<int>(std::fread(buffer, 1, static_cast<std::size_t>(count), hFile)) : 0; }
int CFile::Write(const void *buffer, int count) { return IsOpen() && buffer && count > 0 ? static_cast<int>(std::fwrite(buffer, 1, static_cast<std::size_t>(count), hFile)) : 0; }
int CFile::Seek(int offset, ESeekPosition from) { return IsOpen() && std::fseek(hFile, offset, static_cast<int>(from)) == 0 ? GetPosition() : -1; }
int CFile::GetPosition() const { return IsOpen() ? static_cast<int>(std::ftell(hFile)) : -1; }
int CFile::SetLength(int length) {
    if (!IsOpen() || length < 0) return -1;
    if (std::fflush(hFile) != 0) return -1;
#if defined(_WIN32) || defined(_WIN64)
    if (_chsize_s(_fileno(hFile), length) != 0) return -1;
#else
    if (ftruncate(fileno(hFile), length) != 0) return -1;
#endif
    return length;
}
int CFile::GetLength() const { if (!IsOpen()) return -1; const long current = std::ftell(hFile); if (current < 0 || std::fseek(hFile, 0, SEEK_END) != 0) return -1; const long length = std::ftell(hFile); std::fseek(hFile, current, SEEK_SET); return length < 0 ? -1 : static_cast<int>(length); }
bool CFile::GetStatus(SStatus *status) const {
    if (!IsOpen() || !status) return false;
    std::error_code error;
    const fs::path path(nativePath(szFilePath));
    const auto size = fs::file_size(path, error);
    if (error || size > static_cast<std::uintmax_t>((std::numeric_limits<int>::max)())) return false;
    const fs::directory_entry entry(path, error);
    if (error) return false;
    status->ctime = fileTime(path); status->mtime = fileTime(path); status->atime = status->mtime;
    status->nSize = static_cast<int>(size); status->dwAttributes = attributes(entry); status->szPathName = szFilePath;
    return true;
}
DWORD CFile::GetAttributes(const char *name) { std::error_code error; fs::directory_entry entry(nativePath(name ? name : ""), error); return error ? 0 : attributes(entry); }
bool CFile::SetAttributes(const char *name, DWORD value) { std::error_code error; fs::path path(nativePath(name ? name : "")); auto perms = fs::status(path, error).permissions(); if (error) return false; if (value & readOnly) perms &= ~fs::perms::owner_write; else perms |= fs::perms::owner_write; fs::permissions(path, perms, fs::perm_options::replace, error); return !error; }
bool CFile::Rename(const char *old_name, const char *new_name) { std::error_code error; fs::rename(nativePath(old_name), nativePath(new_name), error); return !error; }
bool CFile::Remove(const char *name) { std::error_code error; return fs::remove(nativePath(name), error) && !error; }
bool CFile::SetFileTime(const FILETIME *, const FILETIME *, const FILETIME *) { return false; }
const std::string &CFile::GetFilePath() const { return szFilePath; }
const std::string CFile::GetFileName() const { return fs::path(nativePath(szFilePath)).filename().string(); }
const std::string CFile::GetFileTitle() const { const std::string n = GetFileName(); const std::size_t p = n.rfind('.'); return p == std::string::npos ? n : n.substr(0, p); }
const std::string CFile::GetFileExt() const { const std::string n = GetFileName(); const std::size_t p = n.rfind('.'); return p == std::string::npos ? "" : n.substr(p + 1); }
bool CFile::SetFileTime(const char *name, const FILETIME *a, const FILETIME *b, const FILETIME *c) { CFile file(name, modeWrite); return file.IsOpen() && file.SetFileTime(a, b, c); }

const CFileIterator &CFileIterator::FindFirstFile(const char *mask) {
    Close(); entries.clear(); index = 0;
    szPath = splitDirectory(mask ? mask : "", szMask);
    std::error_code error;
    for (fs::directory_iterator it(nativePath(szPath), error); !error && it != fs::directory_iterator(); it.increment(error)) {
        if (wildcard(szMask, it->path().filename().string())) {
            Entry entry{it->path().filename().string(), legacyPath(fs::absolute(it->path(), error).lexically_normal().string()), attributes(*it), fileTime(it->path()), fileTime(it->path()), fileTime(it->path()), 0};
            std::error_code size_error; const auto size = it->is_regular_file(size_error) ? fs::file_size(it->path(), size_error) : 0;
            entry.size = size_error || size > static_cast<std::uintmax_t>((std::numeric_limits<int>::max)()) ? 0 : static_cast<int>(size);
            entries.push_back(std::move(entry));
        }
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.name < b.name; });
    return *this;
}
const CFileIterator &CFileIterator::Next() { if (!IsEnd()) ++index; return *this; }
bool CFileIterator::Close() { entries.clear(); index = 0; return true; }
const std::string CFileIterator::GetFileTitle() const { const std::string n = GetFileName(); const std::size_t p = n.rfind('.'); return p == std::string::npos ? n : n.substr(0, p); }
const std::string CFileIterator::GetFileExt() const { const std::string n = GetFileName(); const std::size_t p = n.rfind('.'); return p == std::string::npos ? "" : n.substr(p + 1); }

void DeleteFiles(const char *start, const char *mask, bool recursive) { EnumerateFiles(start, mask, [](const CFileIterator &it) { CFile::Remove(it.GetFilePath().c_str()); }, recursive); }
void DeleteDirectory(const char *path) { std::error_code error; fs::remove_all(nativePath(path), error); }
void CreatePath(const char *path) { std::error_code error; fs::create_directories(nativePath(path ? path : ""), error); }
class CDirFileEnum { std::list<std::string> *names; bool dirs, files; public: CDirFileEnum(std::list<std::string> *n, bool d, bool f) : names(n), dirs(d), files(f) {} void operator()(const CFileIterator &it) { if (it.IsDirectory() ? dirs : files) names->push_back(it.GetFilePath()); } };
void GetDirNames(const char *path, std::list<std::string> *names, bool recursive) { EnumerateFiles(path, "*", CDirFileEnum(names, true, false), recursive); }
void GetFileNames(const char *path, const char *mask, std::list<std::string> *names, bool recursive) { EnumerateFiles(path, mask, CDirFileEnum(names, false, true), recursive); }
bool IsFileExist(const char *name) { std::error_code error; return fs::is_regular_file(nativePath(name ? name : ""), error) && !error; }
std::string GetFullName(const std::string &path) { std::error_code error; return legacyPath(fs::absolute(nativePath(path), error).lexically_normal().string()); }
double GetFreeDiskSpace(const char *path) { std::error_code error; const auto info = fs::space(nativePath(path ? path : "."), error); return error ? 0.0 : static_cast<double>(info.available); }
}
