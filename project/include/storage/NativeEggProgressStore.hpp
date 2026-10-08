#pragma once
#include "storage/NativeEggProgress.hpp"
namespace Pokerogue3DS {
// Uses the existing journal backend and caller-owned scratch. Returned views
// borrow scratch until the next store operation. A global coordinator must bind
// the committed egg generation to run/profile progress before gameplay uses it.
class NativeEggProgressStore {
public:
    NativeEggProgressStore(NativeSaveStorage& storage,char* scratch,size_t capacity,size_t slotCapacity)
        :m_storage(storage),m_scratch(scratch),m_capacity(capacity),m_slotCapacity(slotCapacity) {}
    bool overlapsWorkspace(const void* bytes,size_t size) const {
        return StarterCandyProfileCodec::overlaps(m_scratch,m_capacity,bytes,size);
    }
    NativeSaveResult load(const char* hash,NativeEggProgressView& output,uint32_t generation=0) {
        if(m_scratch && StarterCandyProfileCodec::overlaps(m_scratch,m_capacity,&output,sizeof(output)))
            return NativeSaveResult::InvalidRecord;
        int selected=-1;auto status=select(hash,selected);
        if(status!=NativeSaveResult::Ok) return status;
        if(generation) {
            selected=-1;
            for(unsigned i=0;i<2;++i) if(m_valid[i] && m_generations[i]==generation) selected=int(i);
            if(selected<0) return NativeSaveResult::NotFound;
        }
        return inspectNativeEggProgress(slot(selected),m_sizes[selected],hash,output);
    }
    NativeSaveResult inspectGeneration(const char* hash,uint32_t generation) {
        if(!generation) return NativeSaveResult::InvalidRecord;
        NativeEggProgressView view{};return load(hash,view,generation);
    }
    // Explicit generation only: never export a prepared/latest state by accident.
    // Standalone egg transport; the global portable bundle must still include it.
    NativeSaveResult exportGeneration(const char* hash,uint32_t generation) {
        if(!generation) return NativeSaveResult::InvalidRecord;
        NativeEggProgressView view{};auto status=load(hash,view,generation);
        if(status!=NativeSaveResult::Ok) return status;
        const unsigned selected=view.inventoryBytes==slot(0)+view.headerBytes ? 0 : 1;
        const size_t size=m_sizes[selected];char expected[64];
        std::memcpy(expected,slot(selected)+size-64,64);
        status=m_storage.writeExport(slot(selected),size);if(status!=NativeSaveResult::Ok) return status;
        size_t read=0;status=m_storage.readExport(slot(1-selected),m_slotCapacity,read);
        if(status!=NativeSaveResult::Ok) return status;
        if(read!=size || std::memcmp(expected,slot(1-selected)+read-64,64)) return NativeSaveResult::ChecksumMismatch;
        NativeEggProgressView verified{};status=inspectNativeEggProgress(slot(1-selected),read,hash,verified);
        if(status!=NativeSaveResult::Ok) return status;
        return verified.generation==generation ? NativeSaveResult::Ok : NativeSaveResult::InvalidRecord;
    }
    NativeSaveResult prepare(const EggIncubationRecord* eggs,size_t count,const uint32_t (&vouchers)[4],
        const EggPityState& pity,const char* hash,uint32_t committedGeneration,uint32_t& preparedGeneration) {
        return prepareImpl(eggs,count,vouchers,pity,nullptr,hash,committedGeneration,preparedGeneration);
    }
    // Explicit unlock ledger authorizes v1-to-v2 migration; never infer legacy
    // counters from rarity. Both APIs use the same committed-slot discipline.
    NativeSaveResult prepare(const EggIncubationRecord* eggs,size_t count,const uint32_t (&vouchers)[4],
        const EggPityState& pity,const uint32_t (&unlockPity)[4],const char* hash,
        uint32_t committedGeneration,uint32_t& preparedGeneration) {
        return prepareImpl(eggs,count,vouchers,pity,&unlockPity,hash,committedGeneration,preparedGeneration);
    }
    // Only the global coordinator may use this after proving the authoritative
    // run has eggProgressGeneration zero (or no run exists). Pending slots then
    // have no committed owner and may be replaced on bootstrap retry.
    NativeSaveResult prepareUnreferenced(const EggIncubationRecord* eggs,size_t count,
        const uint32_t (&vouchers)[4],const EggPityState& pity,const uint32_t (&unlockPity)[4],
        const char* hash,uint32_t& preparedGeneration) {
        return prepareImpl(eggs,count,vouchers,pity,&unlockPity,hash,0,preparedGeneration,true);
    }
private:
    NativeSaveResult prepareImpl(const EggIncubationRecord* eggs,size_t count,const uint32_t (&vouchers)[4],
        const EggPityState& pity,const uint32_t (*unlockPity)[4],const char* hash,
        uint32_t committedGeneration,uint32_t& preparedGeneration,bool unreferenced=false) {
        if(!StarterCandyProfileCodec::validHash(hash) || count>SIZE_MAX/sizeof(*eggs) || (count && !eggs))
            return NativeSaveResult::InvalidRecord;
        if(unlockPity) {
            for(const auto counter:*unlockPity) if(counter>kEggUnlockPityCap) return NativeSaveResult::InvalidRecord;
            if(StarterCandyProfileCodec::overlaps(m_scratch,m_capacity,unlockPity,sizeof(*unlockPity))
                || StarterCandyProfileCodec::overlaps(unlockPity,sizeof(*unlockPity),&preparedGeneration,sizeof(preparedGeneration)))
                return NativeSaveResult::InvalidRecord;
        }
        if(StarterCandyProfileCodec::overlaps(eggs,count*sizeof(*eggs),&preparedGeneration,sizeof(preparedGeneration))
            || StarterCandyProfileCodec::overlaps(vouchers,sizeof(vouchers),&preparedGeneration,sizeof(preparedGeneration))
            || StarterCandyProfileCodec::overlaps(&pity,sizeof(pity),&preparedGeneration,sizeof(preparedGeneration))
            || StarterCandyProfileCodec::overlaps(hash,65,&preparedGeneration,sizeof(preparedGeneration)))
            return NativeSaveResult::InvalidRecord;
        if(m_scratch && (StarterCandyProfileCodec::overlaps(m_scratch,m_capacity,eggs,count*sizeof(*eggs)) ||
            StarterCandyProfileCodec::overlaps(m_scratch,m_capacity,vouchers,sizeof(vouchers)) ||
            StarterCandyProfileCodec::overlaps(m_scratch,m_capacity,&pity,sizeof(pity)) ||
            StarterCandyProfileCodec::overlaps(m_scratch,m_capacity,hash,65) ||
            StarterCandyProfileCodec::overlaps(m_scratch,m_capacity,&preparedGeneration,sizeof(preparedGeneration))))
            return NativeSaveResult::InvalidRecord;
        int selected=-1;auto status=select(hash,selected);
        if(status!=NativeSaveResult::Ok && status!=NativeSaveResult::NotFound) return status;
        // The legacy prepare API has no unlock ledger. Refuse a downgrade that
        // would silently erase counters from any valid newer journal slot.
        for(unsigned i=0;i<2;++i) if(!unlockPity && m_valid[i] && std::memcmp(slot(i),"P3EGGP02",8)==0)
            return NativeSaveResult::UnsupportedVersion;
        unsigned target=0;uint32_t next=1;
        if(selected>=0) {
            if(!committedGeneration && !unreferenced) return NativeSaveResult::InvalidRecord;
            if(m_generations[selected]==UINT32_MAX) return NativeSaveResult::SequenceExhausted;
            next=m_generations[selected]+1;
            if(!committedGeneration) target=unsigned(selected==0 ? 1 : 0);
            else {
                int committed=-1;
                for(unsigned i=0;i<2;++i) if(m_valid[i] && m_generations[i]==committedGeneration) committed=int(i);
                if(committed<0) return NativeSaveResult::NotFound;
                target=unsigned(committed==0?1:0); // Retry never replaces committed state.
            }
        } else if(committedGeneration) return NativeSaveResult::NotFound;
        size_t written=0;
        status=unlockPity
            ? encodeNativeEggProgress(eggs,count,vouchers,pity,*unlockPity,next,hash,slot(target),m_slotCapacity,written)
            : encodeNativeEggProgress(eggs,count,vouchers,pity,next,hash,slot(target),m_slotCapacity,written);
        if(status!=NativeSaveResult::Ok) return status;
        char expected[64];std::memcpy(expected,slot(target)+written-64,64);
        status=m_storage.writeSlot(target,slot(target),written);if(status!=NativeSaveResult::Ok) return status;
        size_t read=0;status=m_storage.readSlot(target,slot(target),m_slotCapacity,read);
        if(status!=NativeSaveResult::Ok) return status;
        if(read!=written || std::memcmp(expected,slot(target)+read-64,64)) return NativeSaveResult::ChecksumMismatch;
        NativeEggProgressView verified{};status=inspectNativeEggProgress(slot(target),read,hash,verified);
        if(status!=NativeSaveResult::Ok) return status;
        if(verified.generation!=next) return NativeSaveResult::InvalidRecord;
        preparedGeneration=next;return NativeSaveResult::Ok;
    }
private:
    char* slot(unsigned i) {return m_scratch+i*m_slotCapacity;}
    NativeSaveResult select(const char* hash,int& selected) {
        if(!StarterCandyProfileCodec::validHash(hash)) return NativeSaveResult::InvalidRecord;
        if(!m_scratch || m_slotCapacity<kEggProgressOverhead+kEggInventoryHeaderBytes ||
            m_slotCapacity>kNativeSaveMaxBytes || m_slotCapacity>m_capacity/2) return NativeSaveResult::TooLarge;
        if(StarterCandyProfileCodec::overlaps(m_scratch,m_capacity,hash,65)) return NativeSaveResult::InvalidRecord;
        selected=-1;NativeSaveResult statuses[2]{};
        for(unsigned i=0;i<2;++i) {
            m_valid[i]=false;m_sizes[i]=0;m_generations[i]=0;
            statuses[i]=m_storage.readSlot(i,slot(i),m_slotCapacity,m_sizes[i]);
            if(statuses[i]==NativeSaveResult::IoError || statuses[i]==NativeSaveResult::TooLarge) return statuses[i];
            if(statuses[i]!=NativeSaveResult::Ok) continue;
            if(m_sizes[i]>m_slotCapacity) return NativeSaveResult::TooLarge;
            if(m_sizes[i]<kEggProgressOverhead+kEggInventoryHeaderBytes) {
                statuses[i]=NativeSaveResult::InvalidFormat;continue;
            }
            char digest[65]{};IntegritySha256::hashHex(slot(i),m_sizes[i]-64,digest);
            if(std::memcmp(digest,slot(i)+m_sizes[i]-64,64)) {
                statuses[i]=NativeSaveResult::ChecksumMismatch;continue;
            }
            NativeEggProgressView view{};statuses[i]=inspectNativeEggProgress(slot(i),m_sizes[i],hash,view);
            if(statuses[i]==NativeSaveResult::UnsupportedVersion || statuses[i]==NativeSaveResult::ContentMismatch)
                return statuses[i];
            if(statuses[i]!=NativeSaveResult::Ok) continue;
            m_valid[i]=true;m_generations[i]=view.generation;
            if(selected<0 || view.generation>m_generations[selected]) selected=int(i);
        }
        if(selected<0) return statuses[0]==NativeSaveResult::NotFound ? statuses[1] : statuses[0];
        if(m_valid[0] && m_valid[1] && m_generations[0]==m_generations[1] &&
            (m_sizes[0]!=m_sizes[1] || std::memcmp(slot(0),slot(1),m_sizes[0]))) return NativeSaveResult::AmbiguousJournal;
        return NativeSaveResult::Ok;
    }
    NativeSaveStorage& m_storage;char* m_scratch;size_t m_capacity,m_slotCapacity;
    size_t m_sizes[2]{};uint32_t m_generations[2]{};bool m_valid[2]{};
};
class SdNativeEggProgressStorage final : public NativeSaveStorage {
public:
    NativeSaveResult readSlot(unsigned,char*,size_t,size_t&) override;
    NativeSaveResult writeSlot(unsigned,const char*,size_t) override;
    NativeSaveResult readExport(char*,size_t,size_t&) override;
    NativeSaveResult writeExport(const char*,size_t) override;
};
}
