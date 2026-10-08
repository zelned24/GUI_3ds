#pragma once
#include "game/EggIncubation.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
#include <cstring>
namespace Pokerogue3DS {
// Versioned component. The enclosing progress journal must bind content hash,
// generation and checksum; these bytes alone are not a complete save file.
inline constexpr size_t kEggInventoryHeaderBytes=12;
inline constexpr size_t kEggInventoryRecordBytes=24;
namespace EggInventoryCodec {
inline EggIncubationRecord record(const char* p) {
    EggIncubationRecord egg{};
    egg.id=StarterCandyProfileCodec::get(p,4);
    egg.speciesDex=static_cast<uint16_t>(StarterCandyProfileCodec::get(p+4,2));
    egg.tier=static_cast<EggTier>(static_cast<uint8_t>(p[6]));
    egg.sourceType=static_cast<EggSourceType>(static_cast<uint8_t>(p[7]));
    const uint32_t waves=StarterCandyProfileCodec::get(p+8,4);
    egg.hatchWaves=static_cast<int32_t>(waves<=INT32_MAX ? int64_t(waves) : int64_t(waves)-4294967296LL);
    egg.timestamp=StarterCandyProfileCodec::get64(p+12);
    egg.variantTier=static_cast<VariantTier>(static_cast<uint8_t>(p[20]));
    const uint8_t flags=static_cast<uint8_t>(p[21]);
    egg.isShiny=(flags&1)!=0;egg.overrideHiddenAbility=(flags&2)!=0;
    egg.eggMoveIndex=static_cast<uint8_t>(p[22]);
    return egg;
}
}
inline NativeSaveResult inspectNativeEggInventory(const char* bytes,size_t length,size_t& count) {
    if(!bytes || length<kEggInventoryHeaderBytes) return NativeSaveResult::InvalidFormat;
    if(StarterCandyProfileCodec::overlaps(bytes,length,&count,sizeof(count))) return NativeSaveResult::InvalidRecord;
    if(std::memcmp(bytes,"P3EGGS01",8)) return NativeSaveResult::UnsupportedVersion;
    const size_t entries=StarterCandyProfileCodec::get(bytes+8,4);
    if(entries>(SIZE_MAX-kEggInventoryHeaderBytes)/kEggInventoryRecordBytes ||
        length!=kEggInventoryHeaderBytes+entries*kEggInventoryRecordBytes) return NativeSaveResult::InvalidFormat;
    for(size_t i=0;i<entries;++i) {
        const char* p=bytes+kEggInventoryHeaderBytes+i*kEggInventoryRecordBytes;
        if((static_cast<uint8_t>(p[21])&~3u) || p[23] ||
            validateEggIncubationRecord(EggInventoryCodec::record(p))!=EggIncubationResult::Ok)
            return NativeSaveResult::InvalidRecord;
        const uint32_t id=StarterCandyProfileCodec::get(p,4);
        for(size_t j=0;j<i;++j)
            if(id==StarterCandyProfileCodec::get(bytes+kEggInventoryHeaderBytes+j*kEggInventoryRecordBytes,4))
                return NativeSaveResult::InvalidRecord;
    }
    count=entries;return NativeSaveResult::Ok;
}
inline NativeSaveResult encodeNativeEggInventory(const EggIncubationRecord* eggs,size_t count,
    char* output,size_t capacity,size_t& written) {
    if(!output || (count && !eggs)) return NativeSaveResult::InvalidRecord;
    if(count>UINT32_MAX || count>(SIZE_MAX-kEggInventoryHeaderBytes)/kEggInventoryRecordBytes ||
        count>SIZE_MAX/sizeof(*eggs)) return NativeSaveResult::TooLarge;
    const size_t length=kEggInventoryHeaderBytes+count*kEggInventoryRecordBytes;
    if(capacity<length) return NativeSaveResult::TooLarge;
    if(StarterCandyProfileCodec::overlaps(output,length,eggs,count*sizeof(*eggs)) ||
        StarterCandyProfileCodec::overlaps(output,length,&written,sizeof(written)) ||
        StarterCandyProfileCodec::overlaps(eggs,count*sizeof(*eggs),&written,sizeof(written))) return NativeSaveResult::InvalidRecord;
    for(size_t i=0;i<count;++i) {
        if(validateEggIncubationRecord(eggs[i])!=EggIncubationResult::Ok) return NativeSaveResult::InvalidRecord;
        for(size_t j=0;j<i;++j) if(eggs[i].id==eggs[j].id) return NativeSaveResult::InvalidRecord;
    }
    std::memcpy(output,"P3EGGS01",8);StarterCandyProfileCodec::put(static_cast<uint32_t>(count),output+8,4);
    for(size_t i=0;i<count;++i) {
        const auto& egg=eggs[i];char* p=output+kEggInventoryHeaderBytes+i*kEggInventoryRecordBytes;
        StarterCandyProfileCodec::put(egg.id,p,4);StarterCandyProfileCodec::put(egg.speciesDex,p+4,2);
        p[6]=static_cast<char>(egg.tier);p[7]=static_cast<char>(egg.sourceType);
        StarterCandyProfileCodec::put(static_cast<uint32_t>(egg.hatchWaves),p+8,4);
        StarterCandyProfileCodec::put64(egg.timestamp,p+12);p[20]=static_cast<char>(egg.variantTier);
        p[21]=static_cast<char>((egg.isShiny?1:0)|(egg.overrideHiddenAbility?2:0));
        p[22]=static_cast<char>(egg.eggMoveIndex);p[23]=0;
    }
    written=length;return NativeSaveResult::Ok;
}
inline NativeSaveResult decodeNativeEggInventory(const char* bytes,size_t length,
    EggIncubationRecord* output,size_t capacity,size_t& count) {
    size_t entries=0;const auto status=inspectNativeEggInventory(bytes,length,entries);
    if(status!=NativeSaveResult::Ok) return status;
    if(entries>capacity) return NativeSaveResult::TooLarge;
    if(entries && !output) return NativeSaveResult::InvalidRecord;
    if(entries>SIZE_MAX/sizeof(*output) ||
        StarterCandyProfileCodec::overlaps(bytes,length,output,entries*sizeof(*output)) ||
        StarterCandyProfileCodec::overlaps(output,entries*sizeof(*output),&count,sizeof(count)) ||
        StarterCandyProfileCodec::overlaps(bytes,length,&count,sizeof(count))) return NativeSaveResult::InvalidRecord;
    for(size_t i=0;i<entries;++i) output[i]=EggInventoryCodec::record(bytes+kEggInventoryHeaderBytes+i*kEggInventoryRecordBytes);
    count=entries;return NativeSaveResult::Ok;
}
}
