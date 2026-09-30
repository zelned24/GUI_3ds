#pragma once
#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

// Presentation packages are independent of saves and executable code. New
// gameplay capabilities require a new ABI; a pack must match the catalog in use.
constexpr uint32_t kContentPackAbi = 1;
constexpr uint32_t kContentPackEntries = 512;
constexpr uint32_t kContentPackMaximumBytes = 64 * 1024 * 1024;
constexpr uint32_t kContentPackManifestMaximum = 132 + 296 * kContentPackEntries + 256;

struct ContentPackEntry {
    char id[96];
    uint8_t hash[32];
    uint32_t size;
    uint16_t width, height;
    char sourcePath[128];
    uint8_t sourceHash[32];
};
struct ContentPackManifest {
    uint32_t release = 0, count = 0, bytes = 0;
    char catalogHash[65]{};
    char assetsRevision[41]{};
    char digest[65]{};
    ContentPackEntry entries[kContentPackEntries]{};
};

enum class ContentUpdateResult : uint8_t {
    Ok, NoInstalledPack, AlreadyCurrent, Incompatible, InvalidManifest,
    InvalidSignature, IntegrityFailure, StorageFailure, InsufficientSpace,
    NetworkFailure, Cancelled, TrustUnavailable
};

// Relative paths are constructed by this module. A platform backend roots them
// in sdmc:/3ds/pokerogue and never receives paths from a remote manifest.
class ContentStorage {
public:
    virtual ~ContentStorage() = default;
    virtual bool read(const char* path, uint32_t offset, void* output,
                      uint32_t capacity, uint32_t& actual) = 0;
    virtual bool write(const char* path, uint32_t offset, const void* bytes,
                       uint32_t size, bool truncate) = 0;
    virtual bool directory(const char* path) = 0;
    virtual uint64_t freeBytes() = 0;
};

// Indexed catalog reads reuse ContentStorage. Paths must be locally resolved
// from a verified release; these helpers do not authenticate a package.
inline bool contentRangeValid(uint32_t fileBytes, uint32_t offset, uint32_t size) {
    return offset <= fileBytes && size <= fileBytes - offset;
}
inline bool contentTableRecordOffset(uint32_t fileBytes, uint32_t tableOffset,
    uint32_t recordCount, uint32_t stride, uint32_t index, uint32_t& output) {
    if (!stride || index >= recordCount || tableOffset > fileBytes ||
        recordCount > (fileBytes - tableOffset) / stride) return false;
    // Full table bounds prove both multiplication and addition are safe.
    output = tableOffset + index * stride;
    return true;
}
inline bool readContentTableRecord(ContentStorage& storage, const char* trustedPath,
    uint32_t fileBytes, uint32_t tableOffset, uint32_t recordCount, uint32_t stride,
    uint32_t index, void* output, uint32_t capacity) {
    uint32_t offset = 0, actual = 0;
    if (!trustedPath || !*trustedPath || !output || capacity < stride ||
        !contentTableRecordOffset(fileBytes, tableOffset, recordCount, stride, index, offset))
        return false;
    // Caller discards output on failure: a backend may partially fill it.
    return storage.read(trustedPath, offset, output, stride, actual) && actual == stride;
}

using ContentSignatureVerifier = bool (*)(const uint8_t digest[32], const uint8_t signature[256], void* context);
// Transport is restricted to a configured HTTPS origin by the platform backend.
// Download must close/flush the file, enforce the byte limit and report failures.
using ContentDownloader = ContentUpdateResult (*)(const char* remotePath, const char* destination,
                                                  uint32_t maximumBytes, void* context);

class ContentUpdateStore {
public:
    ContentUpdateResult load(ContentStorage&, const char* catalogHash,
                             ContentSignatureVerifier, void* verifierContext);
    // Stages a release and writes the inactive activation slot last. Rendering
    // keeps the previous in-memory manifest until load() is called on the UI thread.
    ContentUpdateResult install(ContentStorage&, const char* catalogHash,
                                ContentSignatureVerifier, void* verifierContext,
                                ContentDownloader, void* downloaderContext);
    bool assetPath(const char* id, char* output, std::size_t capacity) const;
    uint32_t release() const { return m_active.release; }
    const ContentPackManifest& manifest() const { return m_active; }

    static ContentUpdateResult readManifest(ContentStorage&, const char* path,
                                            const char* catalogHash, ContentPackManifest&,
                                            ContentSignatureVerifier, void* verifierContext);
private:
    ContentPackManifest m_active{};
    // Reused workspace: bounded allocation, no per-entry heap or giant stack frame.
    ContentPackManifest m_pending{};
    int m_activeSlot = -1;
};

ContentUpdateStore& contentUpdateStore();
const char* contentUpdateMessage(ContentUpdateResult);

} // namespace Pokerogue3DS
