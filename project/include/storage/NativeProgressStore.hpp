#pragma once
#include "storage/NativeStarterCandyStore.hpp"
#include "storage/NativeProgressBundle.hpp"
#include "storage/NativeEggProgressStore.hpp"
#include <memory>
#include <new>

namespace Pokerogue3DS {

// The run is the commit record. Profile writes precede it and preserve the
// generation referenced by the current run until the new run becomes durable.
// One owner must serialize all writes to the run and component journals.
class NativeProgressStore {
public:
    NativeProgressStore(NativeRunSaveStore& runs, NativeStarterCandyStore& profiles)
        : m_runs(runs), m_profiles(profiles) { m_runs.bindStarterProfiles(profiles); }
    NativeProgressStore(NativeRunSaveStore& runs,NativeStarterCandyStore& profiles,NativeEggProgressStore& eggs)
        :m_runs(runs),m_profiles(profiles),m_eggs(&eggs) {
        m_runs.bindStarterProfiles(profiles);m_runs.bindEggProgress(eggs);
    }


    NativeSaveResult load(const char* hash, NativeRunSave& run,
        NativeStarterCandyRecord* records, size_t capacity, size_t& count) {
        std::unique_ptr<NativeRunSave> candidateStorage(new (std::nothrow) NativeRunSave{});
        if (!candidateStorage) return NativeSaveResult::MemoryUnavailable;
        auto& candidate = *candidateStorage;
        auto status = m_runs.load(hash, candidate);
        if (status != NativeSaveResult::Ok) return status;
        if (!candidate.starterProfileGeneration) return NativeSaveResult::InvalidRecord;
        if(candidate.eggProgressGeneration) return NativeSaveResult::UnsupportedVersion;
        size_t candidateCount = 0;
        uint32_t generation = 0;
        status = m_profiles.loadGeneration(hash, candidate.starterProfileGeneration,
            records, capacity, candidateCount, generation);
        if (status != NativeSaveResult::Ok) return status;
        run = candidate;
        count = candidateCount;
        return NativeSaveResult::Ok;
    }

    // Publish a complete owned snapshot only after all referenced generations
    // decode. Staging is bounded by the canonical catalogue and actual inventory.
    // Generation zero preserves legacy absence; it is not a confirmed empty save.
    NativeSaveResult load(const char* hash,NativeRunSave& run,
        NativeStarterCandyRecord* records,size_t capacity,size_t& count,
        EggIncubationRecord* eggs,size_t eggCapacity,size_t& eggCount,NativeEggProgressState& eggState) {
        if(capacity>PokerogueContent::kSpeciesCount || eggCapacity>SIZE_MAX/sizeof(*eggs)
            || (capacity && !records) || (eggCapacity && !eggs)) return NativeSaveResult::InvalidRecord;
        const void* ranges[]={&run,records,&count,eggs,&eggCount,&eggState};
        const size_t sizes[]={sizeof(run),capacity*sizeof(*records),sizeof(count),
            eggCapacity*sizeof(*eggs),sizeof(eggCount),sizeof(eggState)};
        for(unsigned i=0;i<6;++i) {
            if(m_profiles.overlapsWorkspace(ranges[i],sizes[i])
                || (m_eggs && m_eggs->overlapsWorkspace(ranges[i],sizes[i]))) return NativeSaveResult::InvalidRecord;
            if(StarterCandyProfileCodec::overlaps(hash,65,ranges[i],sizes[i])) return NativeSaveResult::InvalidRecord;
            for(unsigned j=0;j<i;++j)
                if(StarterCandyProfileCodec::overlaps(ranges[i],sizes[i],ranges[j],sizes[j])) return NativeSaveResult::InvalidRecord;
        }
        std::unique_ptr<NativeRunSave> candidateStorage(new(std::nothrow) NativeRunSave{});
        if(!candidateStorage) return NativeSaveResult::MemoryUnavailable;
        auto& candidate=*candidateStorage;auto status=m_runs.load(hash,candidate);
        if(status!=NativeSaveResult::Ok) return status;
        if(!candidate.starterProfileGeneration) return NativeSaveResult::InvalidRecord;
        NativeEggProgressView view{};
        if(candidate.eggProgressGeneration) {
            if(!m_eggs) return NativeSaveResult::InvalidRecord;
            status=m_eggs->load(hash,view,candidate.eggProgressGeneration);
            if(status!=NativeSaveResult::Ok) return status;
        }
        if(view.eggCount>eggCapacity) return NativeSaveResult::TooLarge;
        std::unique_ptr<EggIncubationRecord[]> eggStage;
        size_t stagedEggCount=0;
        if(view.eggCount) {
            eggStage.reset(new(std::nothrow) EggIncubationRecord[view.eggCount]{});
            if(!eggStage) return NativeSaveResult::MemoryUnavailable;
            status=decodeNativeEggInventory(view.inventoryBytes,view.inventorySize,eggStage.get(),view.eggCount,stagedEggCount);
            if(status!=NativeSaveResult::Ok) return status;
        }
        const NativeEggProgressState stagedEggState=static_cast<const NativeEggProgressState&>(view);
        std::unique_ptr<NativeStarterCandyRecord[]> profileStage(new(std::nothrow) NativeStarterCandyRecord[capacity ? capacity : 1]{});
        if(!profileStage) return NativeSaveResult::MemoryUnavailable;
        size_t stagedCount=0;uint32_t generation=0;
        status=m_profiles.loadGeneration(hash,candidate.starterProfileGeneration,profileStage.get(),capacity,stagedCount,generation);
        if(status!=NativeSaveResult::Ok) return status;
        if(generation!=candidate.starterProfileGeneration) return NativeSaveResult::InvalidRecord;
        if(stagedCount) std::memcpy(records,profileStage.get(),stagedCount*sizeof(*records));
        if(stagedEggCount) std::memcpy(eggs,eggStage.get(),stagedEggCount*sizeof(*eggs));
        run=candidate;eggState=stagedEggState;count=stagedCount;eggCount=stagedEggCount;
        return NativeSaveResult::Ok;
    }

    // On failure, reload before retrying: an I/O error may occur after the new
    // run reached disk. Outputs remain unchanged; persisted authority may differ.
    // Zero-reference runs bootstrap a profile, without claiming legacy rewards.
    NativeSaveResult commit(NativeRunSave& run, const NativeStarterCandyRecord* records, size_t count) {
        auto status = validateNativeRunSave(run, run.contentHash);
        if (status != NativeSaveResult::Ok) return status;
        std::unique_ptr<NativeRunSave> previousStorage(new (std::nothrow) NativeRunSave{});
        if (!previousStorage) return NativeSaveResult::MemoryUnavailable;
        auto& previous = *previousStorage;
        status = m_runs.load(run.contentHash, previous);
        if (status != NativeSaveResult::Ok && status != NativeSaveResult::NotFound) return status;
        const uint32_t committed = status == NativeSaveResult::Ok ? previous.starterProfileGeneration : 0;
        if (run.starterProfileGeneration != committed || run.eggProgressGeneration !=
            (status==NativeSaveResult::Ok ? previous.eggProgressGeneration : 0)) return NativeSaveResult::InvalidRecord;
        uint32_t prepared = 0;
        status = committed
            ? m_profiles.prepareFromCommitted(records, count, run.contentHash, committed, prepared)
            : m_profiles.save(records, count, run.contentHash, prepared);
        if (status != NativeSaveResult::Ok) return status;
        // The prior envelope is no longer needed. Reuse its bounded storage
        // for the candidate and readback instead of stacking three envelopes.
        previous = run;
        previous.starterProfileGeneration = prepared;
        status = m_runs.save(previous);
        if (status != NativeSaveResult::Ok) return status;
        // NativeRunSaveStore assigns the journal generation; read its committed
        // envelope rather than publishing the caller's stale generation.
        previous = {};
        status = m_runs.load(run.contentHash, previous);
        if (status != NativeSaveResult::Ok) return status;
        if (previous.starterProfileGeneration != prepared) return NativeSaveResult::InvalidRecord;
        run = previous;
        return NativeSaveResult::Ok;
    }

    // Three-component transaction: prepare eggs/profile, commit the run last.
    // Failure leaves caller snapshot unchanged. Reload authoritative run before
    // retry: the last durable write may have succeeded despite an I/O error.
    NativeSaveResult commit(NativeRunSave& run,const NativeStarterCandyRecord* records,size_t count,
        const EggIncubationRecord* eggs,size_t eggCount,const uint32_t (&vouchers)[4],
        const EggPityState& pity,const uint32_t (&unlockPity)[4]) {
        if(!m_eggs || count>PokerogueContent::kSpeciesCount || (count && !records)
            || eggCount>SIZE_MAX/sizeof(*eggs) || (eggCount && !eggs)) return NativeSaveResult::InvalidRecord;
        if(StarterCandyProfileCodec::overlaps(&run,sizeof(run),records,count*sizeof(*records))
            || StarterCandyProfileCodec::overlaps(&run,sizeof(run),eggs,eggCount*sizeof(*eggs))
            || StarterCandyProfileCodec::overlaps(&run,sizeof(run),vouchers,sizeof(vouchers))
            || StarterCandyProfileCodec::overlaps(&run,sizeof(run),&pity,sizeof(pity))
            || StarterCandyProfileCodec::overlaps(&run,sizeof(run),unlockPity,sizeof(unlockPity)))
            return NativeSaveResult::InvalidRecord;
        auto status=validateNativeRunSave(run,run.contentHash);if(status!=NativeSaveResult::Ok) return status;
        uint16_t prior=0;
        for(size_t i=0;i<count;++i) {
            if(!StarterCandyProfileCodec::valid(records[i],prior,PokerogueContent::kMaxStarterCandyCount))
                return NativeSaveResult::InvalidRecord;
            prior=records[i].speciesDex;
        }
        std::unique_ptr<NativeRunSave> storage(new(std::nothrow) NativeRunSave{});
        if(!storage) return NativeSaveResult::MemoryUnavailable;
        auto& candidate=*storage;
        status=m_runs.load(run.contentHash,candidate);
        if(status!=NativeSaveResult::Ok && status!=NativeSaveResult::NotFound) return status;
        const uint32_t profileGeneration=status==NativeSaveResult::Ok ? candidate.starterProfileGeneration : 0;
        const uint32_t eggGeneration=status==NativeSaveResult::Ok ? candidate.eggProgressGeneration : 0;
        if(run.starterProfileGeneration!=profileGeneration || run.eggProgressGeneration!=eggGeneration)
            return NativeSaveResult::InvalidRecord;
        uint32_t preparedEgg=0,preparedProfile=0;
        status=eggGeneration ? m_eggs->prepare(eggs,eggCount,vouchers,pity,unlockPity,run.contentHash,eggGeneration,preparedEgg)
            : m_eggs->prepareUnreferenced(eggs,eggCount,vouchers,pity,unlockPity,run.contentHash,preparedEgg);
        if(status!=NativeSaveResult::Ok) return status;
        status=profileGeneration ? m_profiles.prepareFromCommitted(records,count,run.contentHash,profileGeneration,preparedProfile)
            : m_profiles.save(records,count,run.contentHash,preparedProfile);
        if(status!=NativeSaveResult::Ok) return status;
        candidate=run;candidate.starterProfileGeneration=preparedProfile;candidate.eggProgressGeneration=preparedEgg;
        status=m_runs.save(candidate);if(status!=NativeSaveResult::Ok) return status;
        candidate={};status=m_runs.load(run.contentHash,candidate);if(status!=NativeSaveResult::Ok) return status;
        if(candidate.starterProfileGeneration!=preparedProfile || candidate.eggProgressGeneration!=preparedEgg)
            return NativeSaveResult::InvalidRecord;
        run=candidate;return NativeSaveResult::Ok;
    }

    // Both exports are verified, but these two files are not an atomic portable
    // bundle. Linked standalone imports remain rejected until paired import exists.
    NativeSaveResult exportLatest(const char* hash) {
        std::unique_ptr<NativeRunSave> runStorage(new (std::nothrow) NativeRunSave{});
        if (!runStorage) return NativeSaveResult::MemoryUnavailable;
        auto& run = *runStorage;
        auto status = m_runs.load(hash, run);
        if (status != NativeSaveResult::Ok) return status;
        if (!run.starterProfileGeneration) return NativeSaveResult::InvalidRecord;
        status = m_profiles.exportGeneration(hash, run.starterProfileGeneration);
        return status == NativeSaveResult::Ok ? m_runs.exportLatest(hash) : status;
    }

    // Caller-owned workspace: two bundle capacities, kept outside the ARM11 stack.
    NativeSaveResult exportBundle(NativeProgressBundleStorage& storage, const char* hash,
        char* workspace, size_t workspaceSize, NativeStarterCandyRecord* records, size_t capacity) {
        if (!workspace || workspaceSize < 2 * kNativeProgressBundleMaxBytes)
            return NativeSaveResult::TooLarge;
        if (capacity > PokerogueContent::kSpeciesCount || (capacity && !records)
            || m_profiles.overlapsWorkspace(workspace,workspaceSize)
            || (m_eggs && m_eggs->overlapsWorkspace(workspace,workspaceSize))
            || (records && m_eggs && m_eggs->overlapsWorkspace(records,capacity*sizeof(*records))) ||
            (hash && StarterCandyProfileCodec::overlaps(workspace, workspaceSize, hash, 65)) ||
            (records && StarterCandyProfileCodec::overlaps(workspace, workspaceSize,
                records, capacity * sizeof(*records)))) return NativeSaveResult::InvalidRecord;
        std::unique_ptr<NativeRunSave> runStorage(new (std::nothrow) NativeRunSave{});
        if (!runStorage) return NativeSaveResult::MemoryUnavailable;
        auto& run = *runStorage;
        size_t count = 0, runSize = 0, profileSize = 0, bundleSize = 0;
        auto status=m_runs.load(hash,run);if(status!=NativeSaveResult::Ok) return status;
        if(!run.starterProfileGeneration) return NativeSaveResult::InvalidRecord;
        uint32_t profileGeneration=0;
        status=m_profiles.loadGeneration(hash,run.starterProfileGeneration,records,capacity,count,profileGeneration);
        if(status!=NativeSaveResult::Ok) return status;
        NativeEggProgressView eggView{};size_t eggSize=0;
        char* eggBytes=workspace+kNativeSaveMaxBytes+kStarterCandyProfileMaxBytes;
        if(run.eggProgressGeneration) {
            if(!m_eggs) return NativeSaveResult::InvalidRecord;
            status=m_eggs->load(hash,eggView,run.eggProgressGeneration);if(status!=NativeSaveResult::Ok) return status;
            eggSize=eggView.headerBytes+eggView.inventorySize+64;
            if(eggSize>kNativeSaveMaxBytes) return NativeSaveResult::TooLarge;
            // Preserve the validated component bytes, including legacy metadata.
            std::memcpy(eggBytes,eggView.inventoryBytes-eggView.headerBytes,eggSize);
        }
        status = encodeNativeRunSave(run, workspace, kNativeSaveMaxBytes, runSize);
        if (status != NativeSaveResult::Ok) return status;
        status = encodeNativeStarterCandyProfile(records, count, run.starterProfileGeneration,
            hash, PokerogueContent::kMaxStarterCandyCount, workspace + kNativeSaveMaxBytes,
            kStarterCandyProfileMaxBytes, profileSize);
        if (status != NativeSaveResult::Ok) return status;
        char* bundle = workspace + kNativeProgressBundleMaxBytes;
        status=eggSize
            ? encodeNativeProgressBundle(workspace,runSize,workspace+kNativeSaveMaxBytes,profileSize,eggBytes,eggSize,
                hash,run,bundle,kNativeProgressBundleMaxBytes,bundleSize)
            : encodeNativeProgressBundle(workspace,runSize,workspace+kNativeSaveMaxBytes,profileSize,
                hash,run,bundle,kNativeProgressBundleMaxBytes,bundleSize);
        if (status != NativeSaveResult::Ok) return status;
        status = storage.writeBundle(bundle, bundleSize);
        if (status != NativeSaveResult::Ok) return status;
        size_t read = 0;
        status = storage.readBundle(workspace, kNativeProgressBundleMaxBytes, read);
        if (status != NativeSaveResult::Ok) return status;
        if (read != bundleSize || std::memcmp(workspace, bundle, read)) return NativeSaveResult::ChecksumMismatch;
        NativeProgressBundleView view{};
        return inspectNativeProgressBundle(workspace, read, hash, run, view);
    }

    // Staging only: caller must reconstruct the runtime before committing this pair.
    NativeSaveResult readBundleCandidate(NativeProgressBundleStorage& storage, const char* hash,
        char* workspace, size_t workspaceSize, NativeRunSave& candidate,
        NativeStarterCandyRecord* records, size_t capacity, size_t& count) {
        if (!workspace || workspaceSize < kNativeProgressBundleMaxBytes) return NativeSaveResult::TooLarge;
        if (capacity > PokerogueContent::kSpeciesCount ||
            (hash && StarterCandyProfileCodec::overlaps(workspace, workspaceSize, hash, 65)) ||
            StarterCandyProfileCodec::overlaps(workspace, workspaceSize, &candidate, sizeof(candidate)) ||
            (records && (StarterCandyProfileCodec::overlaps(workspace, workspaceSize,
                records, capacity * sizeof(*records)) || StarterCandyProfileCodec::overlaps(
                &candidate, sizeof(candidate), records, capacity * sizeof(*records)))))
            return NativeSaveResult::InvalidRecord;
        size_t read = 0;
        auto status = storage.readBundle(workspace, kNativeProgressBundleMaxBytes, read);
        if (status != NativeSaveResult::Ok) return status;
        NativeProgressBundleView view{};
        status = inspectNativeProgressBundle(workspace, read, hash, candidate, view);
        if (status != NativeSaveResult::Ok) return status;
        if(view.eggSize) return NativeSaveResult::UnsupportedVersion;
        uint32_t generation = 0;
        return decodeNativeStarterCandyProfile(view.profileBytes, view.profileSize, hash,
            PokerogueContent::kMaxStarterCandyCount, records, capacity, count, generation);
    }

    // Staging for complete portable progress. No journal writes or live-state
    // publication occur until every component has decoded into owned storage.
    NativeSaveResult readBundleCandidate(NativeProgressBundleStorage& storage,const char* hash,
        char* workspace,size_t workspaceSize,NativeRunSave& candidate,
        NativeStarterCandyRecord* records,size_t capacity,size_t& count,
        EggIncubationRecord* eggs,size_t eggCapacity,size_t& eggCount,NativeEggProgressState& eggState) {
        if(!workspace || workspaceSize<kNativeProgressBundleMaxBytes) return NativeSaveResult::TooLarge;
        if(capacity>PokerogueContent::kSpeciesCount || eggCapacity>SIZE_MAX/sizeof(*eggs)
            || (capacity && !records) || (eggCapacity && !eggs)) return NativeSaveResult::InvalidRecord;
        const void* ranges[]={&candidate,records,&count,eggs,&eggCount,&eggState};
        const size_t sizes[]={sizeof(candidate),capacity*sizeof(*records),sizeof(count),
            eggCapacity*sizeof(*eggs),sizeof(eggCount),sizeof(eggState)};
        if(StarterCandyProfileCodec::overlaps(workspace,workspaceSize,hash,65)
            || m_profiles.overlapsWorkspace(workspace,workspaceSize)
            || (m_eggs && m_eggs->overlapsWorkspace(workspace,workspaceSize))) return NativeSaveResult::InvalidRecord;
        for(unsigned i=0;i<6;++i) {
            if(StarterCandyProfileCodec::overlaps(workspace,workspaceSize,ranges[i],sizes[i])
                || StarterCandyProfileCodec::overlaps(hash,65,ranges[i],sizes[i])
                || m_profiles.overlapsWorkspace(ranges[i],sizes[i])
                || (m_eggs && m_eggs->overlapsWorkspace(ranges[i],sizes[i]))) return NativeSaveResult::InvalidRecord;
            for(unsigned j=0;j<i;++j)
                if(StarterCandyProfileCodec::overlaps(ranges[i],sizes[i],ranges[j],sizes[j])) return NativeSaveResult::InvalidRecord;
        }
        size_t read=0;auto status=storage.readBundle(workspace,kNativeProgressBundleMaxBytes,read);
        if(status!=NativeSaveResult::Ok) return status;
        std::unique_ptr<NativeRunSave> stagedRun(new(std::nothrow) NativeRunSave{});
        if(!stagedRun) return NativeSaveResult::MemoryUnavailable;
        NativeProgressBundleView view{};status=inspectNativeProgressBundle(workspace,read,hash,*stagedRun,view);
        if(status!=NativeSaveResult::Ok) return status;
        size_t profileCount=0;uint32_t generation=0;
        status=inspectNativeStarterCandyProfile(view.profileBytes,view.profileSize,hash,
            PokerogueContent::kMaxStarterCandyCount,profileCount,generation);
        if(status!=NativeSaveResult::Ok) return status;
        NativeEggProgressView eggView{};
        if(view.eggSize) {
            status=inspectNativeEggProgress(view.eggBytes,view.eggSize,hash,eggView);
            if(status!=NativeSaveResult::Ok) return status;
        }
        if(profileCount>capacity || eggView.eggCount>eggCapacity) return NativeSaveResult::TooLarge;
        std::unique_ptr<NativeStarterCandyRecord[]> stagedProfile;
        std::unique_ptr<EggIncubationRecord[]> stagedEggs;
        size_t decodedCount=0,decodedEggs=0;
        if(profileCount) {
            stagedProfile.reset(new(std::nothrow) NativeStarterCandyRecord[profileCount]{});
            if(!stagedProfile) return NativeSaveResult::MemoryUnavailable;
            status=decodeNativeStarterCandyProfile(view.profileBytes,view.profileSize,hash,PokerogueContent::kMaxStarterCandyCount,
                stagedProfile.get(),profileCount,decodedCount,generation);
            if(status!=NativeSaveResult::Ok) return status;
        }
        if(eggView.eggCount) {
            stagedEggs.reset(new(std::nothrow) EggIncubationRecord[eggView.eggCount]{});
            if(!stagedEggs) return NativeSaveResult::MemoryUnavailable;
            status=decodeNativeEggInventory(eggView.inventoryBytes,eggView.inventorySize,stagedEggs.get(),eggView.eggCount,decodedEggs);
            if(status!=NativeSaveResult::Ok) return status;
        }
        if(decodedCount) std::memcpy(records,stagedProfile.get(),decodedCount*sizeof(*records));
        if(decodedEggs) std::memcpy(eggs,stagedEggs.get(),decodedEggs*sizeof(*eggs));
        candidate=*stagedRun;eggState=static_cast<const NativeEggProgressState&>(eggView);
        count=decodedCount;eggCount=decodedEggs;return NativeSaveResult::Ok;
    }

    // Foreign journal generations are references inside the bundle, never local IDs.
    // Caller has already replayed the staged runtime. On error reload local authority.
    NativeSaveResult commitImported(NativeRunSave& candidate,
        const NativeStarterCandyRecord* records, size_t count) {
        if(candidate.eggProgressGeneration) return NativeSaveResult::UnsupportedVersion;
        std::unique_ptr<NativeRunSave> localStorage(new (std::nothrow) NativeRunSave{});
        if (!localStorage) return NativeSaveResult::MemoryUnavailable;
        auto& local = *localStorage;
        auto status = m_runs.load(candidate.contentHash, local);
        if (status != NativeSaveResult::Ok && status != NativeSaveResult::NotFound) return status;
        const uint32_t foreignGeneration = candidate.starterProfileGeneration;
        candidate.starterProfileGeneration = status == NativeSaveResult::Ok ? local.starterProfileGeneration : 0;
        status = commit(candidate, records, count);
        if (status != NativeSaveResult::Ok) candidate.starterProfileGeneration = foreignGeneration;
        return status;
    }

    NativeSaveResult commitImported(NativeRunSave& candidate,const NativeStarterCandyRecord* records,size_t count,
        const EggIncubationRecord* eggs,size_t eggCount,NativeEggProgressState& eggState) {
        if(!candidate.eggProgressGeneration || !eggState.unlockPityResolved) return NativeSaveResult::UnsupportedVersion;
        if(eggState.generation!=candidate.eggProgressGeneration || count>PokerogueContent::kSpeciesCount
            || eggCount>SIZE_MAX/sizeof(*eggs) || (count && !records) || (eggCount && !eggs)) return NativeSaveResult::InvalidRecord;
        if(StarterCandyProfileCodec::overlaps(&candidate,sizeof(candidate),&eggState,sizeof(eggState))
            || StarterCandyProfileCodec::overlaps(&candidate,sizeof(candidate),records,count*sizeof(*records))
            || StarterCandyProfileCodec::overlaps(&candidate,sizeof(candidate),eggs,eggCount*sizeof(*eggs))
            || StarterCandyProfileCodec::overlaps(&eggState,sizeof(eggState),records,count*sizeof(*records))
            || StarterCandyProfileCodec::overlaps(&eggState,sizeof(eggState),eggs,eggCount*sizeof(*eggs))) return NativeSaveResult::InvalidRecord;
        std::unique_ptr<NativeRunSave> local(new(std::nothrow) NativeRunSave{});
        if(!local) return NativeSaveResult::MemoryUnavailable;
        auto status=m_runs.load(candidate.contentHash,*local);
        if(status!=NativeSaveResult::Ok && status!=NativeSaveResult::NotFound) return status;
        const uint32_t profileGeneration=status==NativeSaveResult::Ok ? local->starterProfileGeneration : 0;
        const uint32_t eggGeneration=status==NativeSaveResult::Ok ? local->eggProgressGeneration : 0;
        *local=candidate;local->starterProfileGeneration=profileGeneration;local->eggProgressGeneration=eggGeneration;
        status=commit(*local,records,count,eggs,eggCount,eggState.vouchers,eggState.pity,eggState.unlockPity);
        if(status!=NativeSaveResult::Ok) return status;
        candidate=*local;eggState.generation=candidate.eggProgressGeneration;return NativeSaveResult::Ok;
    }

private:
    NativeRunSaveStore& m_runs;
    NativeStarterCandyStore& m_profiles;
    NativeEggProgressStore* m_eggs=nullptr;
};
} // namespace Pokerogue3DS
