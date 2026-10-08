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
        size_t candidateCount = 0;
        uint32_t generation = 0;
        status = m_profiles.loadGeneration(hash, candidate.starterProfileGeneration,
            records, capacity, candidateCount, generation);
        if (status != NativeSaveResult::Ok) return status;
        run = candidate;
        count = candidateCount;
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
        if (capacity > PokerogueContent::kSpeciesCount ||
            (hash && StarterCandyProfileCodec::overlaps(workspace, workspaceSize, hash, 65)) ||
            (records && StarterCandyProfileCodec::overlaps(workspace, workspaceSize,
                records, capacity * sizeof(*records)))) return NativeSaveResult::InvalidRecord;
        std::unique_ptr<NativeRunSave> runStorage(new (std::nothrow) NativeRunSave{});
        if (!runStorage) return NativeSaveResult::MemoryUnavailable;
        auto& run = *runStorage;
        size_t count = 0, runSize = 0, profileSize = 0, bundleSize = 0;
        auto status = load(hash, run, records, capacity, count);
        if (status != NativeSaveResult::Ok) return status;
        status = encodeNativeRunSave(run, workspace, kNativeSaveMaxBytes, runSize);
        if (status != NativeSaveResult::Ok) return status;
        status = encodeNativeStarterCandyProfile(records, count, run.starterProfileGeneration,
            hash, PokerogueContent::kMaxStarterCandyCount, workspace + kNativeSaveMaxBytes,
            kStarterCandyProfileMaxBytes, profileSize);
        if (status != NativeSaveResult::Ok) return status;
        char* bundle = workspace + kNativeProgressBundleMaxBytes;
        status = encodeNativeProgressBundle(workspace, runSize, workspace + kNativeSaveMaxBytes,
            profileSize, hash, run, bundle, kNativeProgressBundleMaxBytes, bundleSize);
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
        uint32_t generation = 0;
        return decodeNativeStarterCandyProfile(view.profileBytes, view.profileSize, hash,
            PokerogueContent::kMaxStarterCandyCount, records, capacity, count, generation);
    }

    // Foreign journal generations are references inside the bundle, never local IDs.
    // Caller has already replayed the staged runtime. On error reload local authority.
    NativeSaveResult commitImported(NativeRunSave& candidate,
        const NativeStarterCandyRecord* records, size_t count) {
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

private:
    NativeRunSaveStore& m_runs;
    NativeStarterCandyStore& m_profiles;
    NativeEggProgressStore* m_eggs=nullptr;
};
} // namespace Pokerogue3DS
