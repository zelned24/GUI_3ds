#pragma once
#include "storage/NativeRunSave.hpp"
#include "storage/IntegritySha256.hpp"
#include "content/WindowTexture.hpp"
#include <cstring>
namespace Pokerogue3DS {
// Device presentation preferences are independent from game progress/catalog hashes.
struct NativePresentationSettings { uint32_t generation=0; unsigned windowStyle=1; };
class NativePresentationSettingsStore {
public:
    explicit NativePresentationSettingsStore(NativeSaveStorage& storage):m_storage(storage) {}
    static constexpr size_t kBytes=52;
    static NativeSaveResult encode(const NativePresentationSettings& value,char (&bytes)[kBytes]) {
        if(!value.generation || !findWindowTexture(value.windowStyle)) return NativeSaveResult::InvalidRecord;
        std::memcpy(bytes,"P3UIPREF",8);put(bytes+8,1);put(bytes+12,value.generation);put(bytes+16,value.windowStyle);
        IntegritySha256 digest;digest.update(bytes,20);digest.finish(reinterpret_cast<uint8_t*>(bytes+20));
        return NativeSaveResult::Ok;
    }
    static NativeSaveResult decode(const char* bytes,size_t size,NativePresentationSettings& output) {
        if(!bytes || size!=kBytes || std::memcmp(bytes,"P3UIPREF",8)!=0) return NativeSaveResult::InvalidFormat;
        uint8_t hash[32];IntegritySha256 digest;digest.update(bytes,20);digest.finish(hash);
        if(std::memcmp(hash,bytes+20,32)!=0) return NativeSaveResult::ChecksumMismatch;
        if(get(bytes+8)!=1) return NativeSaveResult::UnsupportedVersion;
        NativePresentationSettings value{get(bytes+12),get(bytes+16)};
        if(!value.generation || !findWindowTexture(value.windowStyle)) return NativeSaveResult::InvalidRecord;
        output=value;return NativeSaveResult::Ok;
    }
    NativeSaveResult load(NativePresentationSettings& output,bool* recovered=nullptr) {
        NativePresentationSettings values[2];NativeSaveResult results[2];char bytes[kBytes];
        if(recovered) *recovered=false;
        for(unsigned i=0;i<2;++i) {
            size_t size=0;results[i]=m_storage.readSlot(i,bytes,sizeof(bytes),size);
            if(results[i]==NativeSaveResult::Ok) results[i]=decode(bytes,size,values[i]);
            // Preserve unreadable/newer preferences rather than silently overwrite them.
            if(results[i]==NativeSaveResult::IoError || results[i]==NativeSaveResult::UnsupportedVersion ||
                results[i]==NativeSaveResult::TooLarge || results[i]==NativeSaveResult::InvalidRecord) return results[i];
        }
        const bool valid0=results[0]==NativeSaveResult::Ok,valid1=results[1]==NativeSaveResult::Ok;
        if(!valid0 && !valid1) return results[0]!=NativeSaveResult::NotFound ? results[0] : results[1];
        if(valid0 && valid1 && values[0].generation==values[1].generation && values[0].windowStyle!=values[1].windowStyle)
            return NativeSaveResult::AmbiguousJournal;
        const unsigned selected=valid1 && (!valid0 || values[1].generation>values[0].generation) ? 1 : 0;
        if(recovered) *recovered=results[1-selected]!=NativeSaveResult::Ok && results[1-selected]!=NativeSaveResult::NotFound;
        output=values[selected];return NativeSaveResult::Ok;
    }
    NativeSaveResult save(unsigned windowStyle) {
        if(!findWindowTexture(windowStyle)) return NativeSaveResult::InvalidRecord;
        NativePresentationSettings previous;const auto result=load(previous);
        if(result!=NativeSaveResult::Ok && result!=NativeSaveResult::NotFound) return result;
        if(previous.generation==0xffffffffu) return NativeSaveResult::SequenceExhausted;
        NativePresentationSettings next{previous.generation+1,windowStyle};char bytes[kBytes];
        auto status=encode(next,bytes);if(status!=NativeSaveResult::Ok) return status;
        const unsigned slot=(next.generation-1)%2;
        status=m_storage.writeSlot(slot,bytes,sizeof(bytes));if(status!=NativeSaveResult::Ok) return status;
        char checked[kBytes];size_t size=0;status=m_storage.readSlot(slot,checked,sizeof(checked),size);
        if(status!=NativeSaveResult::Ok) return status;
        if(size!=sizeof(bytes) || std::memcmp(bytes,checked,sizeof(bytes))!=0) return NativeSaveResult::ChecksumMismatch;
        return NativeSaveResult::Ok;
    }
private:
    static void put(char* out,uint32_t value) {for(unsigned i=0;i<4;++i) out[i]=static_cast<char>(value>>(i*8));}
    static uint32_t get(const char* in) {uint32_t value=0;for(unsigned i=0;i<4;++i) value|=uint32_t(static_cast<unsigned char>(in[i]))<<(i*8);return value;}
    NativeSaveStorage& m_storage;
};
class SdNativePresentationStorage final:public NativeSaveStorage {
public:
    NativeSaveResult readSlot(unsigned,char*,size_t,size_t&) override;
    NativeSaveResult writeSlot(unsigned,const char*,size_t) override;
    NativeSaveResult readExport(char*,size_t,size_t& size) override {size=0;return NativeSaveResult::UnsupportedStage;}
    NativeSaveResult writeExport(const char*,size_t) override {return NativeSaveResult::UnsupportedStage;}
};
}
