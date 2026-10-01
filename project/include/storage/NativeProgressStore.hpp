#pragma once
#include "storage/NativeStarterCandyStore.hpp"

namespace Pokerogue3DS {

// The run is the commit record. Profile writes precede it and preserve the
// generation referenced by the current run until the new run becomes durable.
// One owner must serialize all writes to these two journals.
class NativeProgressStore {
public:
    NativeProgressStore(NativeRunSaveStore& runs, NativeStarterCandyStore& profiles)
        : m_runs(runs), m_profiles(profiles) {}

    NativeSaveResult load(const char* hash, NativeRunSave& run,
        NativeStarterCandyRecord* records, size_t capacity, size_t& count) {
        NativeRunSave candidate{};
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
        NativeRunSave previous{};
        status = m_runs.load(run.contentHash, previous);
        if (status != NativeSaveResult::Ok && status != NativeSaveResult::NotFound) return status;
        const uint32_t committed = status == NativeSaveResult::Ok ? previous.starterProfileGeneration : 0;
        if (run.starterProfileGeneration != committed) return NativeSaveResult::InvalidRecord;
        uint32_t prepared = 0;
        status = committed
            ? m_profiles.prepareFromCommitted(records, count, run.contentHash, committed, prepared)
            : m_profiles.save(records, count, run.contentHash, prepared);
        if (status != NativeSaveResult::Ok) return status;
        NativeRunSave candidate = run;
        candidate.starterProfileGeneration = prepared;
        status = m_runs.save(candidate);
        if (status != NativeSaveResult::Ok) return status;
        // NativeRunSaveStore assigns the journal generation; read its committed
        // envelope rather than publishing the caller's stale generation.
        NativeRunSave verified{};
        status = m_runs.load(run.contentHash, verified);
        if (status != NativeSaveResult::Ok) return status;
        if (verified.starterProfileGeneration != prepared) return NativeSaveResult::InvalidRecord;
        run = verified;
        return NativeSaveResult::Ok;
    }

private:
    NativeRunSaveStore& m_runs;
    NativeStarterCandyStore& m_profiles;
};
} // namespace Pokerogue3DS
