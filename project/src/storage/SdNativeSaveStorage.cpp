#include "storage/NativeRunSave.hpp"
#include "storage/NativeStarterCandyStore.hpp"
#include "storage/NativeProgressBundle.hpp"
#include <cstdio>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace Pokerogue3DS {
namespace {
bool directory(const char* path) {
    if (mkdir(path, 0777) == 0) return true;
    struct stat info{};
    return errno == EEXIST && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}
NativeSaveResult readFile(const char* path, char* output, size_t capacity, size_t& size) {
    size = 0;
    FILE* file = std::fopen(path, "rb");
    if (!file) return errno == ENOENT ? NativeSaveResult::NotFound : NativeSaveResult::IoError;
    size = std::fread(output, 1, capacity, file);
    const int extra = std::fgetc(file);
    const bool failed = std::ferror(file) != 0;
    const bool closed = std::fclose(file) == 0;
    if (failed || !closed) return NativeSaveResult::IoError;
    return extra == EOF ? NativeSaveResult::Ok : NativeSaveResult::TooLarge;
}
NativeSaveResult writeFile(const char* path, const char* bytes, size_t size, size_t maximum = kNativeSaveMaxBytes) {
    if (size > maximum) return NativeSaveResult::TooLarge;
    if (!directory("sdmc:/3ds") || !directory("sdmc:/3ds/pokerogue") ||
        !directory(SdNativeSaveStorage::kDirectory) || !directory("sdmc:/3ds/pokerogue/exports")) return NativeSaveResult::IoError;
    FILE* file = std::fopen(path, "wb");
    if (!file) return NativeSaveResult::IoError;
    bool ok = std::fwrite(bytes, 1, size, file) == size;
    if (std::fflush(file) != 0) ok = false;
    if (fsync(fileno(file)) != 0) ok = false;
    if (std::fclose(file) != 0) ok = false;
    return ok ? NativeSaveResult::Ok : NativeSaveResult::IoError;
}
const char* slotPath(unsigned slot) {
    return slot == 0 ? "sdmc:/3ds/pokerogue/saves/run0.p3save" : "sdmc:/3ds/pokerogue/saves/run1.p3save";
}
}
NativeSaveResult SdNativeSaveStorage::readSlot(unsigned slot, char* output, size_t capacity, size_t& size) {
    if (slot > 1) return NativeSaveResult::InvalidRecord;
    return readFile(slotPath(slot), output, capacity, size);
}
NativeSaveResult SdNativeSaveStorage::writeSlot(unsigned slot, const char* bytes, size_t size) {
    if (slot > 1) return NativeSaveResult::InvalidRecord;
    return writeFile(slotPath(slot), bytes, size);
}
NativeSaveResult SdNativeSaveStorage::deleteSlot(unsigned slot) {
    if (slot > 1) return NativeSaveResult::InvalidRecord;
    const char* path = slotPath(slot);
    if (std::remove(path) != 0 && errno != ENOENT) {
        return NativeSaveResult::IoError;
    }
    return NativeSaveResult::Ok;
}
NativeSaveResult SdNativeSaveStorage::readExport(char* output, size_t capacity, size_t& size) {
    return readFile(kExportPath, output, capacity, size);
}
NativeSaveResult SdNativeSaveStorage::writeExport(const char* bytes, size_t size) {
    // The journal remains authoritative if an export is interrupted.
    return writeFile(kExportPath, bytes, size);
}
NativeSaveResult SdNativeSaveStorage::readBundle(char* output, size_t capacity, size_t& size) {
    return readFile(kBundlePath, output, capacity, size);
}
NativeSaveResult SdNativeSaveStorage::writeBundle(const char* bytes, size_t size) {
    return writeFile(kBundlePath, bytes, size, kNativeProgressBundleMaxBytes);
}
NativeSaveResult SdNativeStarterCandyStorage::readSlot(unsigned slot, char* output, size_t capacity, size_t& read) {
    if (slot > 1) return NativeSaveResult::InvalidRecord;
    return readFile(slot == 0 ? "sdmc:/3ds/pokerogue/saves/starters0.p3profile"
        : "sdmc:/3ds/pokerogue/saves/starters1.p3profile", output, capacity, read);
}
NativeSaveResult SdNativeStarterCandyStorage::writeSlot(unsigned slot, const char* bytes, size_t length) {
    if (slot > 1) return NativeSaveResult::InvalidRecord;
    return writeFile(slot == 0 ? "sdmc:/3ds/pokerogue/saves/starters0.p3profile"
        : "sdmc:/3ds/pokerogue/saves/starters1.p3profile", bytes, length, kStarterCandyProfileMaxBytes);
}
NativeSaveResult SdNativeStarterCandyStorage::readExport(char* output, size_t capacity, size_t& read) {
    return readFile(kExportPath, output, capacity, read);
}
NativeSaveResult SdNativeStarterCandyStorage::writeExport(const char* bytes, size_t length) {
    return writeFile(kExportPath, bytes, length, kStarterCandyProfileMaxBytes);
}
} // namespace Pokerogue3DS
