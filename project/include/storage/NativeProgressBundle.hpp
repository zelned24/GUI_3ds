#pragma once
#include "storage/NativeStarterCandyProfile.hpp"

namespace Pokerogue3DS {
inline constexpr size_t kNativeProgressBundleOverhead = 80;
inline constexpr size_t kNativeProgressBundleMaxBytes = kNativeProgressBundleOverhead +
    kNativeSaveMaxBytes + kStarterCandyProfileMaxBytes;

// Views borrow the validated input buffer. Scratch is workspace, never live state.
struct NativeProgressBundleView {
    const char* runBytes = nullptr;
    size_t runSize = 0;
    const char* profileBytes = nullptr;
    size_t profileSize = 0;
    uint32_t sourceProfileGeneration = 0;
};

inline NativeSaveResult validateNativeProgressPair(const char* runBytes, size_t runSize,
    const char* profileBytes, size_t profileSize, const char* hash, NativeRunSave& scratch) {
    if (!runBytes || !profileBytes || !runSize || runSize > kNativeSaveMaxBytes ||
        profileSize < kStarterCandyProfileOverhead || profileSize > kStarterCandyProfileMaxBytes ||
        StarterCandyProfileCodec::overlaps(runBytes, runSize, &scratch, sizeof(scratch)) ||
        StarterCandyProfileCodec::overlaps(profileBytes, profileSize, &scratch, sizeof(scratch)) ||
        (hash && StarterCandyProfileCodec::overlaps(hash, 65, &scratch, sizeof(scratch))))
        return NativeSaveResult::InvalidRecord;
    auto status = decodeNativeRunSave(runBytes, runSize, hash, scratch);
    if (status != NativeSaveResult::Ok) return status;
    size_t count = 0;
    uint32_t generation = 0;
    status = inspectNativeStarterCandyProfile(profileBytes, profileSize, hash,
        PokerogueContent::kMaxStarterCandyCount, count, generation);
    if (status != NativeSaveResult::Ok) return status;
    return scratch.starterProfileGeneration && scratch.starterProfileGeneration == generation
        ? NativeSaveResult::Ok : NativeSaveResult::InvalidRecord;
}

// P3PROG01 | LE32 run length | LE32 profile length | run | profile | ASCII SHA256.
// The outer digest binds the complete pair; each existing component is validated.
inline NativeSaveResult inspectNativeProgressBundle(const char* bytes, size_t length, const char* hash,
    NativeRunSave& scratch, NativeProgressBundleView& output) {
    if (!bytes || length < kNativeProgressBundleOverhead || length > kNativeProgressBundleMaxBytes)
        return NativeSaveResult::InvalidFormat;
    if (std::memcmp(bytes, "P3PROG01", 8)) return NativeSaveResult::UnsupportedVersion;
    const size_t runSize = StarterCandyProfileCodec::get(bytes + 8, 4);
    const size_t profileSize = StarterCandyProfileCodec::get(bytes + 12, 4);
    if (!runSize || runSize > kNativeSaveMaxBytes || profileSize < kStarterCandyProfileOverhead ||
        profileSize > kStarterCandyProfileMaxBytes ||
        length != kNativeProgressBundleOverhead + runSize + profileSize)
        return NativeSaveResult::InvalidFormat;
    char digest[65]{};
    IntegritySha256::hashHex(bytes, length - 64, digest);
    if (std::memcmp(digest, bytes + length - 64, 64)) return NativeSaveResult::ChecksumMismatch;
    if (StarterCandyProfileCodec::overlaps(bytes, length, &scratch, sizeof(scratch)))
        return NativeSaveResult::InvalidRecord;
    NativeProgressBundleView view{bytes + 16, runSize, bytes + 16 + runSize, profileSize, 0};
    const auto status = validateNativeProgressPair(view.runBytes, view.runSize, view.profileBytes,
        view.profileSize, hash, scratch);
    if (status != NativeSaveResult::Ok) return status;
    view.sourceProfileGeneration = scratch.starterProfileGeneration;
    output = view;
    return NativeSaveResult::Ok;
}

inline NativeSaveResult encodeNativeProgressBundle(const char* runBytes, size_t runSize,
    const char* profileBytes, size_t profileSize, const char* hash, NativeRunSave& scratch,
    char* output, size_t capacity, size_t& written) {
    written = 0;
    if (!output || !hash || !runBytes || !profileBytes || !runSize || runSize > kNativeSaveMaxBytes ||
        profileSize < kStarterCandyProfileOverhead || profileSize > kStarterCandyProfileMaxBytes)
        return NativeSaveResult::InvalidRecord;
    const size_t size = kNativeProgressBundleOverhead + runSize + profileSize;
    if (capacity < size) return NativeSaveResult::TooLarge;
    if (StarterCandyProfileCodec::overlaps(output, size, runBytes, runSize) ||
        StarterCandyProfileCodec::overlaps(output, size, profileBytes, profileSize) ||
        StarterCandyProfileCodec::overlaps(output, size, hash, 65) ||
        StarterCandyProfileCodec::overlaps(output, size, &scratch, sizeof(scratch)))
        return NativeSaveResult::InvalidRecord;
    const auto status = validateNativeProgressPair(runBytes, runSize, profileBytes, profileSize, hash, scratch);
    if (status != NativeSaveResult::Ok) return status;
    std::memcpy(output, "P3PROG01", 8);
    StarterCandyProfileCodec::put(static_cast<uint32_t>(runSize), output + 8, 4);
    StarterCandyProfileCodec::put(static_cast<uint32_t>(profileSize), output + 12, 4);
    std::memcpy(output + 16, runBytes, runSize);
    std::memcpy(output + 16 + runSize, profileBytes, profileSize);
    char digest[65]{};
    IntegritySha256::hashHex(output, size - 64, digest);
    std::memcpy(output + size - 64, digest, 64);
    written = size;
    return NativeSaveResult::Ok;
}
} // namespace Pokerogue3DS
