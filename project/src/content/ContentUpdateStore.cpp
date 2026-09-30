#include "content/ContentUpdateStore.hpp"
#include "storage/IntegritySha256.hpp"
#include <cstring>

namespace Pokerogue3DS {
namespace {
uint32_t u32(const uint8_t* p) { return p[0] | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | uint16_t(p[1]) << 8); }
void put32(uint8_t* p, uint32_t n) { for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(n >> (i * 8)); }
void hex(const uint8_t* bytes, char* out) {
    const char* digits = "0123456789abcdef";
    for (unsigned i = 0; i < 32; ++i) { out[i * 2] = digits[bytes[i] >> 4]; out[i * 2 + 1] = digits[bytes[i] & 15]; }
    out[64] = 0;
}
bool isHex(const char* value, unsigned size) {
    for (unsigned i = 0; i < size; ++i) if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
    return true;
}
bool textField(const uint8_t* p, unsigned n, bool path) {
    unsigned i = 0;
    for (; i < n && p[i]; ++i) {
        const char c = char(p[i]);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '_' || c == ':' || (path && (c == '/' || c == '.')))) return false;
        if (path && c == '.' && (i == 0 || p[i - 1] == '.' || p[i - 1] == '/')) return false;
    }
    if (!i || i == n || (path && p[0] == '/')) return false;
    for (; i < n; ++i) if (p[i]) return false;
    return true;
}
bool exact(ContentStorage& io, const char* path, uint32_t offset, void* bytes, uint32_t size) {
    uint32_t read = 0;
    return io.read(path, offset, bytes, size, read) && read == size;
}
bool end(ContentStorage& io, const char* path, uint32_t offset) {
    uint8_t byte; uint32_t count = 0;
    return io.read(path, offset, &byte, 1, count) && count == 0;
}
bool join(char* out, std::size_t cap, const char* a, const char* b, const char* c = "") {
    const auto na = std::strlen(a), nb = std::strlen(b), nc = std::strlen(c);
    if (na + nb + nc + 1 > cap) return false;
    std::memcpy(out, a, na); std::memcpy(out + na, b, nb); std::memcpy(out + na + nb, c, nc + 1);
    return true;
}
void releaseDirectory(const char* digest, char out[96]) { join(out, 96, "content/releases/", digest); }
bool assetFile(const char* digest, const uint8_t hash[32], char* out, std::size_t cap) {
    char dir[96], name[70], hashText[65]; releaseDirectory(digest, dir); hex(hash, hashText);
    join(name, sizeof(name), "/", hashText, ".t3x"); return join(out, cap, dir, name);
}
bool verifyFile(ContentStorage& io, const char* path, uint32_t size, const uint8_t expected[32]) {
    uint8_t buffer[4096], hash[32]; IntegritySha256 sha;
    for (uint32_t offset = 0; offset < size;) {
        const uint32_t count = size - offset < sizeof(buffer) ? size - offset : sizeof(buffer);
        if (!exact(io, path, offset, buffer, count)) return false;
        sha.update(buffer, count); offset += count;
    }
    sha.finish(hash); return end(io, path, size) && std::memcmp(hash, expected, 32) == 0;
}
bool validateFiles(ContentStorage& io, const ContentPackManifest& manifest) {
    char file[176];
    for (uint32_t i = 0; i < manifest.count; ++i) {
        const auto& entry = manifest.entries[i];
        if (!assetFile(manifest.digest, entry.hash, file, sizeof(file)) || !verifyFile(io, file, entry.size, entry.hash)) return false;
    }
    return true;
}
bool activation(ContentStorage& io, int slot, char digest[65], uint32_t& release) {
    const char* path = slot == 0 ? "content/active0.bin" : "content/active1.bin";
    uint8_t bytes[108], hash[32];
    if (!exact(io, path, 0, bytes, sizeof(bytes)) || !end(io, path, sizeof(bytes)) || std::memcmp(bytes, "P3ACTIVE", 8) != 0 || !isHex(reinterpret_cast<char*>(bytes + 8), 64)) return false;
    IntegritySha256 sha; sha.update(bytes, 76); sha.finish(hash);
    if (std::memcmp(hash, bytes + 76, 32)) return false;
    std::memcpy(digest, bytes + 8, 64); digest[64] = 0; release = u32(bytes + 72); return release != 0;
}
}

ContentUpdateResult ContentUpdateStore::readManifest(ContentStorage& io, const char* path,
        const char* catalog, ContentPackManifest& out, ContentSignatureVerifier verify, void* context) {
    if (!verify) return ContentUpdateResult::TrustUnavailable;
    uint8_t header[132];
    if (!exact(io, path, 0, header, sizeof(header))) return ContentUpdateResult::InvalidManifest;
    if (std::memcmp(header, "P3UPD001", 8) || u32(header + 8) != 1) return ContentUpdateResult::InvalidManifest;
    if (u32(header + 12) != kContentPackAbi || !catalog || std::strlen(catalog) != 64 || std::memcmp(header + 28, catalog, 64)) return ContentUpdateResult::Incompatible;
    const uint32_t release = u32(header + 16), count = u32(header + 20), bytes = u32(header + 24);
    if (!release || !count || count > kContentPackEntries || !bytes || bytes > kContentPackMaximumBytes ||
        !isHex(reinterpret_cast<char*>(header + 28), 64) || !isHex(reinterpret_cast<char*>(header + 92), 40)) return ContentUpdateResult::InvalidManifest;
    const uint32_t signedSize = sizeof(header) + count * 296;
    if (!end(io, path, signedSize + 256)) return ContentUpdateResult::InvalidManifest;
    IntegritySha256 sha; sha.update(header, sizeof(header));
    uint32_t total = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t record[296];
        if (!exact(io, path, sizeof(header) + i * sizeof(record), record, sizeof(record))) return ContentUpdateResult::InvalidManifest;
        sha.update(record, sizeof(record));
        if (!textField(record, 96, false) || !textField(record + 136, 128, true)) return ContentUpdateResult::InvalidManifest;
        auto& entry = out.entries[i];
        std::memcpy(entry.id, record, 96); std::memcpy(entry.hash, record + 96, 32);
        entry.size = u32(record + 128); entry.width = u16(record + 132); entry.height = u16(record + 134);
        std::memcpy(entry.sourcePath, record + 136, 128); std::memcpy(entry.sourceHash, record + 264, 32);
        if (!entry.size || entry.size > 2 * 1024 * 1024 || !entry.width || !entry.height || entry.width > 1024 || entry.height > 1024 ||
            (i && std::strcmp(out.entries[i - 1].id, entry.id) >= 0) || entry.size > kContentPackMaximumBytes - total) return ContentUpdateResult::InvalidManifest;
        total += entry.size;
    }
    if (total != bytes) return ContentUpdateResult::InvalidManifest;
    uint8_t hash[32], signature[256]; sha.finish(hash);
    if (!exact(io, path, signedSize, signature, sizeof(signature))) return ContentUpdateResult::InvalidManifest;
    if (!verify(hash, signature, context)) return ContentUpdateResult::InvalidSignature;
    sha.update(signature, sizeof(signature)); sha.finish(hash); hex(hash, out.digest);
    out.release = release; out.count = count; out.bytes = bytes;
    std::memcpy(out.catalogHash, header + 28, 64); out.catalogHash[64] = 0;
    std::memcpy(out.assetsRevision, header + 92, 40); out.assetsRevision[40] = 0;
    return ContentUpdateResult::Ok;
}

ContentUpdateResult ContentUpdateStore::load(ContentStorage& io, const char* catalog, ContentSignatureVerifier verify, void* ctx) {
    m_active.count = 0; m_active.release = 0; m_activeSlot = -1;
    ContentUpdateResult failure = ContentUpdateResult::NoInstalledPack;
    for (int slot = 0; slot < 2; ++slot) {
        char digest[65], dir[96], path[120]; uint32_t release;
        if (!activation(io, slot, digest, release)) continue;
        releaseDirectory(digest, dir); join(path, sizeof(path), dir, "/manifest.bin");
        const auto result = readManifest(io, path, catalog, m_pending, verify, ctx);
        if (result != ContentUpdateResult::Ok) { failure = result; continue; }
        if (m_pending.release != release || std::strcmp(m_pending.digest, digest) || !validateFiles(io, m_pending)) { failure = ContentUpdateResult::IntegrityFailure; continue; }
        if (release > m_active.release) { m_active = m_pending; m_activeSlot = slot; }
    }
    return m_active.release ? ContentUpdateResult::Ok : failure;
}

ContentUpdateResult ContentUpdateStore::install(ContentStorage& io, const char* catalog,
        ContentSignatureVerifier verify, void* verifyCtx, ContentDownloader download, void* downloadCtx) {
    if (!verify) return ContentUpdateResult::TrustUnavailable;
    if (!download) return ContentUpdateResult::NetworkFailure;
    if (!io.directory("content") || !io.directory("content/releases")) return ContentUpdateResult::StorageFailure;
    auto result = download("manifest.bin", "content/pending.bin", kContentPackManifestMaximum, downloadCtx);
    if (result != ContentUpdateResult::Ok) return result;
    result = readManifest(io, "content/pending.bin", catalog, m_pending, verify, verifyCtx);
    if (result != ContentUpdateResult::Ok) return result;
    if (m_pending.release < m_active.release) return ContentUpdateResult::Incompatible;
    if (m_pending.release == m_active.release) return std::strcmp(m_pending.digest, m_active.digest) == 0 ? ContentUpdateResult::AlreadyCurrent : ContentUpdateResult::InvalidManifest;
    if (io.freeBytes() < uint64_t(m_pending.bytes) + kContentPackManifestMaximum + 8192) return ContentUpdateResult::InsufficientSpace;
    char dir[96]; releaseDirectory(m_pending.digest, dir);
    if (!io.directory(dir)) return ContentUpdateResult::StorageFailure;
    for (uint32_t i = 0; i < m_pending.count; ++i) {
        const auto& entry = m_pending.entries[i]; char path[176], remote[80], hash[65];
        assetFile(m_pending.digest, entry.hash, path, sizeof(path)); hex(entry.hash, hash);
        if (verifyFile(io, path, entry.size, entry.hash)) continue;
        join(remote, sizeof(remote), "files/", hash, ".t3x");
        result = download(remote, path, entry.size, downloadCtx);
        if (result != ContentUpdateResult::Ok) return result;
        if (!verifyFile(io, path, entry.size, entry.hash)) return ContentUpdateResult::IntegrityFailure;
    }
    char manifestPath[120]; join(manifestPath, sizeof(manifestPath), dir, "/manifest.bin");
    const uint32_t size = 132 + m_pending.count * 296 + 256;
    uint8_t buffer[4096];
    for (uint32_t offset = 0; offset < size;) {
        const auto count = size - offset < sizeof(buffer) ? size - offset : sizeof(buffer);
        if (!exact(io, "content/pending.bin", offset, buffer, count) || !io.write(manifestPath, offset, buffer, count, offset == 0)) return ContentUpdateResult::StorageFailure;
        offset += count;
    }
    // A single torn slot cannot invalidate the previous committed release. The
    // checksum is verified at startup before any pointer is followed.
    uint8_t marker[108]{}; std::memcpy(marker, "P3ACTIVE", 8); std::memcpy(marker + 8, m_pending.digest, 64);
    put32(marker + 72, m_pending.release); IntegritySha256 sha; sha.update(marker, 76); sha.finish(marker + 76);
    const char* slot = m_activeSlot == 0 ? "content/active1.bin" : "content/active0.bin";
    if (!io.write(slot, 0, marker, sizeof(marker), true)) return ContentUpdateResult::StorageFailure;
    uint8_t check[108];
    if (!exact(io, slot, 0, check, sizeof(check)) || std::memcmp(marker, check, sizeof(marker))) return ContentUpdateResult::StorageFailure;
    return ContentUpdateResult::Ok;
}

bool ContentUpdateStore::assetPath(const char* id, char* out, std::size_t capacity) const {
    if (!id || !out) return false;
    for (uint32_t i = 0; i < m_active.count; ++i) if (std::strcmp(id, m_active.entries[i].id) == 0)
        return assetFile(m_active.digest, m_active.entries[i].hash, out, capacity);
    return false;
}
ContentUpdateStore& contentUpdateStore() { static ContentUpdateStore store; return store; }
const char* contentUpdateMessage(ContentUpdateResult result) {
    switch (result) {
    case ContentUpdateResult::Ok: return "Contenido actualizado";
    case ContentUpdateResult::NoInstalledPack: return "Contenido incluido en el juego";
    case ContentUpdateResult::AlreadyCurrent: return "Contenido al dia";
    case ContentUpdateResult::Incompatible: return "Este contenido necesita otro runtime/catalogo";
    case ContentUpdateResult::InvalidManifest: return "Manifiesto no valido";
    case ContentUpdateResult::InvalidSignature: return "Firma de contenido no valida";
    case ContentUpdateResult::IntegrityFailure: return "Contenido incompleto o danado";
    case ContentUpdateResult::StorageFailure: return "No se pudo escribir en la SD";
    case ContentUpdateResult::InsufficientSpace: return "Espacio insuficiente en SD";
    case ContentUpdateResult::NetworkFailure: return "No se pudo descargar por HTTPS";
    case ContentUpdateResult::Cancelled: return "Actualizacion cancelada";
    case ContentUpdateResult::TrustUnavailable: return "Clave de actualizacion no disponible";
    }
    return "Error de contenido";
}
} // namespace Pokerogue3DS
