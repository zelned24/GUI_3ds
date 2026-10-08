#pragma once
#include "storage/NativeStarterCandyProfile.hpp"
#include "storage/NativeEggProgress.hpp"

namespace Pokerogue3DS {
inline constexpr size_t kNativeProgressBundleOverhead = 80;
inline constexpr size_t kNativeProgressTripleOverhead=84;
inline constexpr size_t kNativeProgressBundleMaxBytes=kNativeProgressTripleOverhead+
    2*kNativeSaveMaxBytes+kStarterCandyProfileMaxBytes;

// Views borrow the validated input buffer. Scratch is workspace, never live state.
struct NativeProgressBundleView {
    const char* runBytes = nullptr;
    size_t runSize = 0;
    const char* profileBytes = nullptr;
    size_t profileSize = 0;
    uint32_t sourceProfileGeneration = 0;
    const char* eggBytes=nullptr;
    size_t eggSize=0;
    uint32_t sourceEggGeneration=0;
};

inline NativeSaveResult validateNativeProgressProfileComponents(const char* runBytes, size_t runSize,
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

inline NativeSaveResult validateNativeProgressPair(const char* runBytes,size_t runSize,
    const char* profileBytes,size_t profileSize,const char* hash,NativeRunSave& scratch) {
    const auto status=validateNativeProgressProfileComponents(runBytes,runSize,profileBytes,profileSize,hash,scratch);
    if(status!=NativeSaveResult::Ok) return status;
    return scratch.eggProgressGeneration ? NativeSaveResult::UnsupportedVersion : NativeSaveResult::Ok;
}
inline NativeSaveResult validateNativeProgressTriple(const char* runBytes,size_t runSize,
    const char* profileBytes,size_t profileSize,const char* eggBytes,size_t eggSize,
    const char* hash,NativeRunSave& scratch) {
    if(!eggBytes || eggSize<kEggProgressOverhead+kEggInventoryHeaderBytes || eggSize>kNativeSaveMaxBytes
        || StarterCandyProfileCodec::overlaps(eggBytes,eggSize,&scratch,sizeof(scratch))) return NativeSaveResult::InvalidRecord;
    const auto status=validateNativeProgressProfileComponents(runBytes,runSize,profileBytes,profileSize,hash,scratch);
    if(status!=NativeSaveResult::Ok) return status;
    NativeEggProgressView eggs{};
    const auto inspected=inspectNativeEggProgress(eggBytes,eggSize,hash,eggs);
    if(inspected!=NativeSaveResult::Ok) return inspected;
    return scratch.eggProgressGeneration && scratch.eggProgressGeneration==eggs.generation
        ? NativeSaveResult::Ok : NativeSaveResult::InvalidRecord;
}

// P3PROG01 | LE32 run length | LE32 profile length | run | profile | ASCII SHA256.
// The outer digest binds the complete pair; each existing component is validated.
inline NativeSaveResult inspectNativeProgressBundle(const char* bytes,size_t length,const char* hash,
    NativeRunSave& scratch,NativeProgressBundleView& output) {
    if(!bytes || length<kNativeProgressBundleOverhead || length>kNativeProgressBundleMaxBytes)
        return NativeSaveResult::InvalidFormat;
    if(StarterCandyProfileCodec::overlaps(bytes,length,&scratch,sizeof(scratch))
        || StarterCandyProfileCodec::overlaps(bytes,length,&output,sizeof(output))
        || StarterCandyProfileCodec::overlaps(&scratch,sizeof(scratch),&output,sizeof(output))
        || (hash && StarterCandyProfileCodec::overlaps(hash,65,&output,sizeof(output)))) return NativeSaveResult::InvalidRecord;
    const bool pair=std::memcmp(bytes,"P3PROG01",8)==0,triple=std::memcmp(bytes,"P3PROG02",8)==0;
    if(!pair && !triple) return NativeSaveResult::UnsupportedVersion;
    const size_t header=triple ? 20 : 16,overhead=header+64;
    if(length<overhead) return NativeSaveResult::InvalidFormat;
    const size_t runSize=StarterCandyProfileCodec::get(bytes+8,4),profileSize=StarterCandyProfileCodec::get(bytes+12,4);
    const size_t eggSize=triple ? StarterCandyProfileCodec::get(bytes+16,4) : 0;
    if(!runSize || runSize>kNativeSaveMaxBytes || profileSize<kStarterCandyProfileOverhead
        || profileSize>kStarterCandyProfileMaxBytes || eggSize>kNativeSaveMaxBytes
        || (triple && eggSize<kEggProgressOverhead+kEggInventoryHeaderBytes)
        || length!=overhead+runSize+profileSize+eggSize) return NativeSaveResult::InvalidFormat;
    char digest[65]{};IntegritySha256::hashHex(bytes,length-64,digest);
    if(std::memcmp(digest,bytes+length-64,64)) return NativeSaveResult::ChecksumMismatch;
    NativeProgressBundleView view{bytes+header,runSize,bytes+header+runSize,profileSize,0};
    if(triple) {view.eggBytes=bytes+header+runSize+profileSize;view.eggSize=eggSize;}
    const auto status=triple
        ? validateNativeProgressTriple(view.runBytes,runSize,view.profileBytes,profileSize,view.eggBytes,eggSize,hash,scratch)
        : validateNativeProgressPair(view.runBytes,runSize,view.profileBytes,profileSize,hash,scratch);
    if(status!=NativeSaveResult::Ok) return status;
    view.sourceProfileGeneration=scratch.starterProfileGeneration;view.sourceEggGeneration=scratch.eggProgressGeneration;
    output=view;return NativeSaveResult::Ok;
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
// P3PROG02 adds an egg length and component, bound to the run's exact reference.
inline NativeSaveResult encodeNativeProgressBundle(const char* runBytes,size_t runSize,
    const char* profileBytes,size_t profileSize,const char* eggBytes,size_t eggSize,
    const char* hash,NativeRunSave& scratch,char* output,size_t capacity,size_t& written) {
    if(!output || !hash || !runBytes || !profileBytes || !eggBytes || !runSize || runSize>kNativeSaveMaxBytes
        || profileSize<kStarterCandyProfileOverhead || profileSize>kStarterCandyProfileMaxBytes
        || eggSize<kEggProgressOverhead+kEggInventoryHeaderBytes || eggSize>kNativeSaveMaxBytes)
        return NativeSaveResult::InvalidRecord;
    const size_t length=kNativeProgressTripleOverhead+runSize+profileSize+eggSize;
    if(capacity<length) return NativeSaveResult::TooLarge;
    const void* inputs[]={runBytes,profileBytes,eggBytes,hash,&scratch};
    const size_t sizes[]={runSize,profileSize,eggSize,65,sizeof(scratch)};
    for(unsigned i=0;i<5;++i) {
        if(StarterCandyProfileCodec::overlaps(output,length,inputs[i],sizes[i])
            || StarterCandyProfileCodec::overlaps(&written,sizeof(written),inputs[i],sizes[i])) return NativeSaveResult::InvalidRecord;
    }
    if(StarterCandyProfileCodec::overlaps(output,length,&written,sizeof(written))) return NativeSaveResult::InvalidRecord;
    const auto status=validateNativeProgressTriple(runBytes,runSize,profileBytes,profileSize,eggBytes,eggSize,hash,scratch);
    if(status!=NativeSaveResult::Ok) return status;
    std::memcpy(output,"P3PROG02",8);
    StarterCandyProfileCodec::put(static_cast<uint32_t>(runSize),output+8,4);
    StarterCandyProfileCodec::put(static_cast<uint32_t>(profileSize),output+12,4);
    StarterCandyProfileCodec::put(static_cast<uint32_t>(eggSize),output+16,4);
    std::memcpy(output+20,runBytes,runSize);std::memcpy(output+20+runSize,profileBytes,profileSize);
    std::memcpy(output+20+runSize+profileSize,eggBytes,eggSize);
    char digest[65]{};IntegritySha256::hashHex(output,length-64,digest);std::memcpy(output+length-64,digest,64);
    written=length;return NativeSaveResult::Ok;
}
} // namespace Pokerogue3DS
