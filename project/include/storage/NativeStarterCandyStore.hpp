#pragma once
#include "storage/NativeStarterCandyProfile.hpp"

namespace Pokerogue3DS {

// Caller-owned scratch avoids large journal buffers on the ARM11 stack.
class NativeStarterCandyStore {
public:
    NativeStarterCandyStore(NativeSaveStorage& storage, char* scratch, size_t capacity)
        : m_storage(storage), m_scratch(scratch), m_capacity(capacity) {}

    NativeSaveResult load(const char* hash, uint16_t limit, NativeStarterCandyRecord* records,
        size_t capacity, size_t& count, uint32_t& generation) {
        if (records && capacity && m_scratch && StarterCandyProfileCodec::overlaps(records,
                (capacity < PokerogueContent::kSpeciesCount ? capacity : PokerogueContent::kSpeciesCount) * sizeof(*records),
                m_scratch, 2 * kStarterCandyProfileMaxBytes)) return NativeSaveResult::InvalidRecord;
        int selected = -1;
        const auto status = select(selected);
        if (status != NativeSaveResult::Ok) return status;
        return decodeNativeStarterCandyProfile(slot(selected), m_sizes[selected], hash, limit,
            records, capacity, count, generation);
    }

    NativeSaveResult save(const NativeStarterCandyRecord* records, size_t count,
        const char* hash, uint16_t limit, uint32_t& generation) {
        if (count > PokerogueContent::kSpeciesCount || (count && !records) ||
            !StarterCandyProfileCodec::validHash(hash)) return NativeSaveResult::InvalidRecord;
        if (m_scratch && ((count && StarterCandyProfileCodec::overlaps(records, count * sizeof(*records),
                m_scratch, 2 * kStarterCandyProfileMaxBytes)) || StarterCandyProfileCodec::overlaps(hash, 65,
                m_scratch, 2 * kStarterCandyProfileMaxBytes))) return NativeSaveResult::InvalidRecord;
        int selected = -1;
        auto status = select(selected);
        if (status != NativeSaveResult::Ok && status != NativeSaveResult::NotFound) return status;
        uint32_t next = 1;
        if (selected >= 0) {
            size_t previousCount = 0;
            uint32_t previous = 0;
            status = inspectNativeStarterCandyProfile(slot(selected), m_sizes[selected], hash, limit,
                previousCount, previous);
            if (status != NativeSaveResult::Ok) return status;
            if (previous == 0xffffffffU) return NativeSaveResult::SequenceExhausted;
            next = previous + 1;
        }
        const unsigned target = selected == 0 ? 1 : 0;
        size_t size = 0;
        status = encodeNativeStarterCandyProfile(records, count, next, hash, limit,
            slot(target), kStarterCandyProfileMaxBytes, size);
        if (status != NativeSaveResult::Ok) return status;
        char expectedDigest[64];
        std::memcpy(expectedDigest, slot(target) + size - 64, 64);
        status = m_storage.writeSlot(target, slot(target), size);
        if (status != NativeSaveResult::Ok) return status;
        size_t read = 0;
        status = m_storage.readSlot(target, slot(target), kStarterCandyProfileMaxBytes, read);
        if (status != NativeSaveResult::Ok) return status;
        if (read != size || std::memcmp(expectedDigest, slot(target) + read - 64, 64))
            return NativeSaveResult::ChecksumMismatch;
        size_t verifiedCount = 0;
        uint32_t verified = 0;
        status = inspectNativeStarterCandyProfile(slot(target), read, hash, limit, verifiedCount, verified);
        if (status != NativeSaveResult::Ok) return status;
        if (verified != next || verifiedCount != count) return NativeSaveResult::InvalidRecord;
        generation = next;
        return NativeSaveResult::Ok;
    }

    NativeSaveResult exportLatest(const char* hash, uint16_t limit) {
        int selected = -1;
        auto status = select(selected);
        if (status != NativeSaveResult::Ok) return status;
        size_t count = 0;
        uint32_t generation = 0;
        status = inspectNativeStarterCandyProfile(slot(selected), m_sizes[selected], hash, limit, count, generation);
        return status == NativeSaveResult::Ok
            ? m_storage.writeExport(slot(selected), m_sizes[selected]) : status;
    }

    // Staging is caller-owned workspace, never the live gameplay profile.
    // Import reassigns the local journal generation; the source sequence is not authority.
    NativeSaveResult importExport(const char* hash, NativeStarterCandyRecord* staging,
        size_t capacity, size_t& importedCount, uint32_t& generation) {
        if (!m_scratch || m_capacity < 2 * kStarterCandyProfileMaxBytes) return NativeSaveResult::TooLarge;
        if (!StarterCandyProfileCodec::validHash(hash) ||
            StarterCandyProfileCodec::overlaps(hash, 65, m_scratch, 2 * kStarterCandyProfileMaxBytes))
            return NativeSaveResult::InvalidRecord;
        if (staging && capacity && StarterCandyProfileCodec::overlaps(staging,
                (capacity < PokerogueContent::kSpeciesCount ? capacity : PokerogueContent::kSpeciesCount) * sizeof(*staging),
                m_scratch, 2 * kStarterCandyProfileMaxBytes)) return NativeSaveResult::InvalidRecord;
        size_t read = 0;
        auto status = m_storage.readExport(slot(0), kStarterCandyProfileMaxBytes, read);
        if (status != NativeSaveResult::Ok) return status;
        size_t count = 0;
        uint32_t sourceGeneration = 0;
        status = decodeNativeStarterCandyProfile(slot(0), read, hash, PokerogueContent::kMaxStarterCandyCount,
            staging, capacity, count, sourceGeneration);
        if (status != NativeSaveResult::Ok) return status;
        status = save(staging, count, hash, generation);
        if (status != NativeSaveResult::Ok) return status;
        importedCount = count;
        return NativeSaveResult::Ok;
    }

    // Production callers use the generated pinned limit; explicit-limit overloads
    // remain available for separately resolved overrides and offline fixtures.
    NativeSaveResult load(const char* hash, NativeStarterCandyRecord* records, size_t capacity,
        size_t& count, uint32_t& generation) {
        return load(hash, PokerogueContent::kMaxStarterCandyCount, records, capacity, count, generation);
    }
    NativeSaveResult save(const NativeStarterCandyRecord* records, size_t count,
        const char* hash, uint32_t& generation) {
        return save(records, count, hash, PokerogueContent::kMaxStarterCandyCount, generation);
    }
    NativeSaveResult exportLatest(const char* hash) {
        return exportLatest(hash, PokerogueContent::kMaxStarterCandyCount);
    }

private:
    char* slot(unsigned index) { return m_scratch + index * kStarterCandyProfileMaxBytes; }
    NativeSaveResult select(int& selected) {
        if (!m_scratch || m_capacity < 2 * kStarterCandyProfileMaxBytes) return NativeSaveResult::TooLarge;
        uint32_t sequence[2]{};
        NativeSaveResult statuses[2]{};
        selected = -1;
        for (unsigned i = 0; i < 2; ++i) {
            m_sizes[i] = 0;
            statuses[i] = m_storage.readSlot(i, slot(i), kStarterCandyProfileMaxBytes, m_sizes[i]);
            if (statuses[i] == NativeSaveResult::IoError || statuses[i] == NativeSaveResult::TooLarge)
                return statuses[i];
            if (statuses[i] != NativeSaveResult::Ok) continue;
            const size_t size = m_sizes[i];
            if (size < kStarterCandyProfileOverhead || size > kStarterCandyProfileMaxBytes) {
                statuses[i] = NativeSaveResult::InvalidFormat; continue;
            }
            char digest[65]{};
            IntegritySha256::hashHex(slot(i), size - 64, digest);
            if (std::memcmp(digest, slot(i) + size - 64, 64)) {
                statuses[i] = NativeSaveResult::ChecksumMismatch; continue;
            }
            // A valid but unsupported version must not cause silent rollback.
            if (std::memcmp(slot(i), "P3CANDY1", 8)) return NativeSaveResult::UnsupportedVersion;
            sequence[i] = StarterCandyProfileCodec::get(slot(i) + 72, 4);
            if (!sequence[i]) { statuses[i] = NativeSaveResult::InvalidRecord; continue; }
            if (selected < 0 || sequence[i] > sequence[selected]) selected = static_cast<int>(i);
        }
        if (selected < 0) return statuses[0] == NativeSaveResult::NotFound ? statuses[1] : statuses[0];
        if (sequence[0] && sequence[0] == sequence[1] &&
            (m_sizes[0] != m_sizes[1] || std::memcmp(slot(0), slot(1), m_sizes[0])))
            return NativeSaveResult::AmbiguousJournal;
        return NativeSaveResult::Ok;
    }
    NativeSaveStorage& m_storage;
    char* m_scratch;
    size_t m_capacity;
    size_t m_sizes[2]{};
};

class SdNativeStarterCandyStorage final : public NativeSaveStorage {
public:
    static constexpr const char* kExportPath = "sdmc:/3ds/pokerogue/exports/starters.p3profile";
    NativeSaveResult readSlot(unsigned slot, char* output, size_t capacity, size_t& read) override;
    NativeSaveResult writeSlot(unsigned slot, const char* bytes, size_t length) override;
    NativeSaveResult readExport(char* output, size_t capacity, size_t& read) override;
    NativeSaveResult writeExport(const char* bytes, size_t length) override;
};
} // namespace Pokerogue3DS
