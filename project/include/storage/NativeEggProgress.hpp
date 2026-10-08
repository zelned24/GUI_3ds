#pragma once
#include "storage/NativeEggInventory.hpp"
#include "game/EggGachaPolicy.hpp"
namespace Pokerogue3DS {
struct NativeEggProgressView {
    uint32_t generation=0;
    uint32_t vouchers[4]{};
    EggPityState pity{};
    const char* inventoryBytes=nullptr;
    size_t inventorySize=0,eggCount=0;
};
inline constexpr size_t kEggProgressHeaderBytes=112;
inline constexpr size_t kEggProgressOverhead=176;
// P3EGGP01 | content SHA | generation | four voucher counts | four pity counts |
// inventory length | P3EGGS01 component | envelope SHA. Journal ownership and
// linking its generation to the global progress transaction remain separate.
inline NativeSaveResult inspectNativeEggProgress(const char* bytes,size_t length,
    const char* contentHash,NativeEggProgressView& output) {
    if(!bytes || !StarterCandyProfileCodec::validHash(contentHash) ||
        length<kEggProgressOverhead+kEggInventoryHeaderBytes) return NativeSaveResult::InvalidFormat;
    if(StarterCandyProfileCodec::overlaps(bytes,length,&output,sizeof(output)) ||
        StarterCandyProfileCodec::overlaps(contentHash,65,&output,sizeof(output))) return NativeSaveResult::InvalidRecord;
    if(std::memcmp(bytes,"P3EGGP01",8)) return NativeSaveResult::UnsupportedVersion;
    const size_t inventorySize=StarterCandyProfileCodec::get(bytes+108,4);
    if(inventorySize!=length-kEggProgressOverhead) return NativeSaveResult::InvalidFormat;
    char digest[65]{};IntegritySha256::hashHex(bytes,length-64,digest);
    if(std::memcmp(digest,bytes+length-64,64)) return NativeSaveResult::ChecksumMismatch;
    if(std::memcmp(bytes+8,contentHash,64)) return NativeSaveResult::ContentMismatch;
    NativeEggProgressView candidate{};candidate.generation=StarterCandyProfileCodec::get(bytes+72,4);
    if(!candidate.generation) return NativeSaveResult::InvalidRecord;
    for(unsigned i=0;i<4;++i) candidate.vouchers[i]=StarterCandyProfileCodec::get(bytes+76+i*4,4);
    candidate.pity={StarterCandyProfileCodec::get(bytes+92,4),StarterCandyProfileCodec::get(bytes+96,4),
        StarterCandyProfileCodec::get(bytes+100,4),StarterCandyProfileCodec::get(bytes+104,4)};
    candidate.inventoryBytes=bytes+kEggProgressHeaderBytes;candidate.inventorySize=inventorySize;
    const auto status=inspectNativeEggInventory(candidate.inventoryBytes,inventorySize,candidate.eggCount);
    if(status!=NativeSaveResult::Ok) return status;
    output=candidate;return NativeSaveResult::Ok;
}
inline NativeSaveResult encodeNativeEggProgress(const EggIncubationRecord* eggs,size_t count,
    const uint32_t (&vouchers)[4],const EggPityState& pity,uint32_t generation,const char* contentHash,
    char* output,size_t capacity,size_t& written) {
    if(!output || !generation || !StarterCandyProfileCodec::validHash(contentHash) || (count && !eggs))
        return NativeSaveResult::InvalidRecord;
    if(count>(UINT32_MAX-kEggInventoryHeaderBytes)/kEggInventoryRecordBytes ||
        count>(SIZE_MAX-kEggProgressOverhead-kEggInventoryHeaderBytes)/kEggInventoryRecordBytes)
        return NativeSaveResult::TooLarge;
    const size_t inventorySize=kEggInventoryHeaderBytes+count*kEggInventoryRecordBytes;
    const size_t length=kEggProgressOverhead+inventorySize;
    if(length>capacity) return NativeSaveResult::TooLarge;
    if(count>SIZE_MAX/sizeof(*eggs) ||
        StarterCandyProfileCodec::overlaps(output,length,eggs,count*sizeof(*eggs)) ||
        StarterCandyProfileCodec::overlaps(output,length,vouchers,sizeof(vouchers)) ||
        StarterCandyProfileCodec::overlaps(output,length,&pity,sizeof(pity)) ||
        StarterCandyProfileCodec::overlaps(output,length,contentHash,65) ||
        StarterCandyProfileCodec::overlaps(output,length,&written,sizeof(written)) ||
        StarterCandyProfileCodec::overlaps(vouchers,sizeof(vouchers),&written,sizeof(written)) ||
        StarterCandyProfileCodec::overlaps(&pity,sizeof(pity),&written,sizeof(written)) ||
        StarterCandyProfileCodec::overlaps(eggs,count*sizeof(*eggs),&written,sizeof(written))) return NativeSaveResult::InvalidRecord;
    size_t inventoryWritten=0;
    const auto status=encodeNativeEggInventory(eggs,count,output+kEggProgressHeaderBytes,inventorySize,inventoryWritten);
    if(status!=NativeSaveResult::Ok) return status;
    std::memcpy(output,"P3EGGP01",8);std::memcpy(output+8,contentHash,64);
    StarterCandyProfileCodec::put(generation,output+72,4);
    for(unsigned i=0;i<4;++i) StarterCandyProfileCodec::put(vouchers[i],output+76+i*4,4);
    StarterCandyProfileCodec::put(pity.common,output+92,4);StarterCandyProfileCodec::put(pity.rare,output+96,4);
    StarterCandyProfileCodec::put(pity.epic,output+100,4);StarterCandyProfileCodec::put(pity.legendary,output+104,4);
    StarterCandyProfileCodec::put(static_cast<uint32_t>(inventoryWritten),output+108,4);
    char digest[65]{};IntegritySha256::hashHex(output,length-64,digest);std::memcpy(output+length-64,digest,64);
    written=length;return NativeSaveResult::Ok;
}
}
