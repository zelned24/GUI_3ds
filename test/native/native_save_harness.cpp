#include "storage/NativeRunSave.hpp"
#include "game/PokemonExperience.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
#include "storage/NativeStarterCandyStore.hpp"
#include "storage/NativeProgressStore.hpp"
#include "storage/NativeProgressBundle.hpp"
#include "storage/NativeEggInventory.hpp"
#include "storage/NativeEggProgress.hpp"
#include "storage/NativeEggProgressStore.hpp"
#include "game/EggGachaPolicy.hpp"
#include "storage/IntegritySha256.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstring>

namespace {
using namespace Pokerogue3DS;
class MemoryStorage : public NativeSaveStorage {
public:
    char slots[2][kNativeSaveMaxBytes]{};
    size_t sizes[2]{};
    char exported[kNativeSaveMaxBytes]{};
    size_t exportSize = 0;
    bool interrupt = false;
    NativeSaveResult readSlot(unsigned i, char* out, size_t cap, size_t& size) override {
        size = sizes[i];
        if (!size) return NativeSaveResult::NotFound;
        if (size > cap) return NativeSaveResult::TooLarge;
        std::memcpy(out, slots[i], size); return NativeSaveResult::Ok;
    }
    NativeSaveResult writeSlot(unsigned i, const char* in, size_t size) override {
        sizes[i] = interrupt ? size / 2 : size;
        std::memcpy(slots[i], in, sizes[i]);
        return interrupt ? NativeSaveResult::IoError : NativeSaveResult::Ok;
    }
    NativeSaveResult deleteSlot(unsigned i) override {
        if (i > 1) return NativeSaveResult::InvalidRecord;
        sizes[i] = 0;
        std::memset(slots[i], 0, sizeof(slots[i]));
        return NativeSaveResult::Ok;
    }
    NativeSaveResult readExport(char* out, size_t cap, size_t& size) override {
        size = exportSize;
        if (!size) return NativeSaveResult::NotFound;
        if (size > cap) return NativeSaveResult::TooLarge;
        std::memcpy(out, exported, size); return NativeSaveResult::Ok;
    }
    NativeSaveResult writeExport(const char* in, size_t size) override {
        exportSize = size; std::memcpy(exported, in, size); return NativeSaveResult::Ok;
    }
};
class MemoryBundleStorage : public NativeProgressBundleStorage {
public:
    char bytes[kNativeProgressBundleMaxBytes]{};
    size_t size = 0;
    bool interrupt = false;
    NativeSaveResult readBundle(char* out, size_t capacity, size_t& read) override {
        read = 0;
        if (!size) return NativeSaveResult::NotFound;
        if (size > capacity) return NativeSaveResult::TooLarge;
        std::memcpy(out, bytes, size); read = size; return NativeSaveResult::Ok;
    }
    NativeSaveResult writeBundle(const char* input, size_t length) override {
        if (length > sizeof(bytes)) return NativeSaveResult::TooLarge;
        size = interrupt ? length / 2 : length;
        std::memcpy(bytes, input, size);
        return interrupt ? NativeSaveResult::IoError : NativeSaveResult::Ok;
    }
};
}
static int checkImplicitBaseFormCodec() {
    using namespace Pokerogue3DS;
    const auto* species = PokerogueContent::findSpeciesByDex(10); // Pinned Caterpie has no explicit forms.
    if (!species || species->firstFormId[0]) return 11800;
    PokemonBattleInit input{};
    input.speciesDex = species->dex;
    input.level = 5;
    input.pokemonId = 123;
    input.abilityId = species->ability1;
    input.gender = PokemonGender::Male;
    input.nature = PokemonNature::Hardy;
    input.deriveIvsFromPokemonId = true;
    input.moveCount = 1;
    input.moveIds[0] = 33; // Canonical Tackle injected only for codec regression.
    PokemonBattleState state{};
    if (initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok || state.formId) return 11801;
    uint32_t experience = 0;
    if (pokemonTotalExperienceForLevel(species->growthRate, 5, experience) != PokemonExperienceResult::Ok) return 11802;
    PokemonActorIdentity identity{};
    identity.pokemonId = state.pokemonId;
    identity.gender = state.gender;
    identity.nature = state.nature;
    identity.initialTeraType = resolvePokemonTypeSymbol(species->type1);
    identity.initialTeraTypeResolved = true;
    for (uint8_t i = 0; i < 6; ++i) identity.ivs[i] = state.ivs[i];
    NativePokemonSave saved{}, decoded{};
    if (!captureNativePokemonActorSave(state, identity, experience, saved) || saved.formId[0]) return 11803;
    char bytes[512]{};
    size_t length = 0;
    if (encodeNativePokemonSave(saved, bytes, sizeof(bytes), length) != NativeSaveResult::Ok ||
        decodeNativePokemonSave(bytes, length, decoded) != NativeSaveResult::Ok) return 11804;
    if(decoded.appearanceResolved || decoded.shiny || decoded.shinyVariant) return 11820;
    auto appearanceSaved=saved;appearanceSaved.appearanceResolved=true;appearanceSaved.shiny=true;
    for(uint8_t variant=0;variant<3;++variant) {
        appearanceSaved.shinyVariant=variant;
        if(encodeNativePokemonSave(appearanceSaved,bytes,sizeof(bytes),length)!=NativeSaveResult::Ok ||
            decodeNativePokemonSave(bytes,length,decoded)!=NativeSaveResult::Ok ||
            !decoded.appearanceResolved || !decoded.shiny || decoded.shinyVariant!=variant ||
            decoded.hasEatenBerry!=saved.hasEatenBerry) return 11821;
    }
    appearanceSaved.shiny=false;appearanceSaved.shinyVariant=0;
    if(encodeNativePokemonSave(appearanceSaved,bytes,sizeof(bytes),length)!=NativeSaveResult::Ok ||
        decodeNativePokemonSave(bytes,length,decoded)!=NativeSaveResult::Ok ||
        !decoded.appearanceResolved || decoded.shiny || decoded.shinyVariant) return 11822;
    if(encodeNativePokemonSave(saved,bytes,sizeof(bytes),length)!=NativeSaveResult::Ok ||
        decodeNativePokemonSave(bytes,length,decoded)!=NativeSaveResult::Ok) return 11823;

    PokemonBattleState restored{};
    PokemonActorIdentity restoredIdentity{};
    if (!restoreNativePokemonActorSave(decoded, restored, restoredIdentity) || restored.formId ||
        restoredIdentity.formId || restored.speciesDex != species->dex || restored.hp != state.hp ||
        restored.moves[0].pp != state.moves[0].pp ||
        std::strcmp(restoredIdentity.initialTeraType, identity.initialTeraType)) return 11805;
    auto invalid = decoded;
    std::memcpy(invalid.formId, "not_a_canonical_form", sizeof("not_a_canonical_form"));
    if (restoreNativePokemonActorSave(invalid, restored, restoredIdentity) || restored.speciesDex != species->dex)
        return 11806;
    invalid = decoded;
    invalid.speciesDex = 3; // Venusaur has explicit catalog forms; empty must not alias them.
    const auto* explicitSpecies = PokerogueContent::findSpeciesByDex(invalid.speciesDex);
    if (!explicitSpecies || !explicitSpecies->firstFormId[0]) return 11808;
    if (restoreNativePokemonActorSave(invalid, restored, restoredIdentity)) return 11807;
    return 0;
}

extern "C" int runNativeSaveChecks() {
    // Egg component coverage belongs to the existing save gate. This component
    // has not yet been connected to the durable profile/run journals.
    {
        for(unsigned source=0;source<=static_cast<unsigned>(EggSourceType::EVENT);++source) {
            unsigned distribution[4]{};
            for(unsigned roll=0;roll<256;++roll) {
                EggTier tier=EggTier::COMMON;
                if(eggTierForRoll(roll,static_cast<EggSourceType>(source),tier)!=EggIncubationResult::Ok) return 1220;
                ++distribution[static_cast<unsigned>(tier)];
            }
            const bool legendary=source==static_cast<unsigned>(EggSourceType::GACHA_LEGENDARY);
            if(distribution[0]!=(legendary?203u:204u) || distribution[1]!=44 || distribution[2]!=7 ||
                distribution[3]!=(legendary?2u:1u)) return 1221;
        }
        EggTierPityPlan pityPlan{};
        EggPityState pity{7,kEggPityThresholds.rare-1,kEggPityThresholds.epic-1,kEggPityThresholds.legendary-1};
        if(planEggTierPity(EggTier::COMMON,EggSourceType::GACHA_MOVE,pity,pityPlan)!=EggPityResult::Ok ||
            pityPlan.tier!=EggTier::LEGENDARY || pityPlan.pity.legendary ||
            pityPlan.pity.epic!=kEggPityThresholds.epic || pityPlan.pity.rare!=kEggPityThresholds.rare ||
            pityPlan.pity.common!=7) return 1223;
        if(planEggTierPity(EggTier::RARE,EggSourceType::GACHA_LEGENDARY,pity,pityPlan)!=EggPityResult::Ok ||
            pityPlan.tier!=EggTier::RARE || pityPlan.pity.rare ||
            pityPlan.pity.legendary!=kEggPityThresholds.legendary+1) return 1224;
        pity.legendary=0;
        if(planEggTierPity(EggTier::COMMON,EggSourceType::GACHA_MOVE,pity,pityPlan)!=EggPityResult::Ok ||
            pityPlan.tier!=EggTier::EPIC || pityPlan.pity.epic || pityPlan.pity.rare!=kEggPityThresholds.rare) return 1225;
        pity.epic=0;
        if(planEggTierPity(EggTier::COMMON,EggSourceType::GACHA_MOVE,pity,pityPlan)!=EggPityResult::Ok ||
            pityPlan.tier!=EggTier::RARE || pityPlan.pity.rare || pityPlan.pity.epic!=1) return 1226;
        pity.rare=0;
        if(planEggTierPity(EggTier::COMMON,EggSourceType::GACHA_MOVE,pity,pityPlan)!=EggPityResult::Ok ||
            pityPlan.tier!=EggTier::COMMON || pityPlan.pity.common || pityPlan.pity.rare!=1) return 1227;
        pity.rare=UINT32_MAX;
        const auto unchanged=pityPlan;
        if(planEggTierPity(EggTier::COMMON,EggSourceType::GACHA_MOVE,pity,pityPlan)!=EggPityResult::CounterOverflow ||
            pityPlan.tier!=unchanged.tier || pityPlan.pity.rare!=unchanged.pity.rare) return 1228;
        uint32_t vouchers[4]={50,50,50,50};EggVoucherPlan voucherPlan{};
        for(const auto& offer:kEggVoucherOffers) {
            if(planEggVoucherPull(offer.cursor,0,vouchers,voucherPlan)!=EggVoucherResult::Ok ||
                voucherPlan.voucher!=offer.voucher || voucherPlan.remaining!=50-offer.consumed ||
                voucherPlan.pulls!=offer.pulls) return 1229;
            const auto before=voucherPlan;
            if(planEggVoucherPull(offer.cursor,kEggGachaInventoryLimit,vouchers,voucherPlan)!=EggVoucherResult::InventoryFull ||
                voucherPlan.remaining!=before.remaining || voucherPlan.pulls!=before.pulls) return 1230;
            if(planEggVoucherPull(offer.cursor,kEggGachaInventoryLimit-offer.pulls,vouchers,voucherPlan)!=EggVoucherResult::Ok)
                return 1231;
        }
        uint32_t emptyVouchers[4]{};
        if(planEggVoucherPull(0,0,emptyVouchers,voucherPlan)!=EggVoucherResult::InsufficientVouchers ||
            planEggVoucherPull(0,kEggGachaInventoryLimit,emptyVouchers,voucherPlan)!=EggVoucherResult::InventoryFull)
            return 1232;
        if(planEggVoucherPull(0,UINT32_MAX,emptyVouchers,voucherPlan,true)!=EggVoucherResult::Ok ||
            voucherPlan.remaining || voucherPlan.pulls!=1) return 1233;
        if(planEggVoucherPull(5,0,vouchers,voucherPlan)!=EggVoucherResult::Cancelled ||
            planEggVoucherPull(6,0,vouchers,voucherPlan)!=EggVoucherResult::InvalidOption || vouchers[0]!=50)
            return 1234;
        EggTier invalidOutput=EggTier::EPIC;
        if(eggTierForRoll(256,EggSourceType::GACHA_MOVE,invalidOutput)!=EggIncubationResult::InvalidInput ||
            invalidOutput!=EggTier::EPIC || eggTierForRoll(0,static_cast<EggSourceType>(255),invalidOutput)
            !=EggIncubationResult::InvalidInput || invalidOutput!=EggTier::EPIC) return 1222;
        {
            EggIncubationRecord envelopeEgg{};envelopeEgg.id=123;envelopeEgg.hatchWaves=10;
            const uint32_t balances[4]={1,2,3,4};const EggPityState pityLedger{5,6,7,8};
            char envelope[212]{},repeat[212]{};size_t envelopeSize=0,repeatSize=0;
            if(encodeNativeEggProgress(&envelopeEgg,1,balances,pityLedger,17,PokerogueContent::kContentHash,
                envelope,sizeof(envelope),envelopeSize)!=NativeSaveResult::Ok || envelopeSize!=212 ||
                encodeNativeEggProgress(&envelopeEgg,1,balances,pityLedger,17,PokerogueContent::kContentHash,
                repeat,sizeof(repeat),repeatSize)!=NativeSaveResult::Ok || repeatSize!=envelopeSize ||
                std::memcmp(envelope,repeat,envelopeSize)) return 1235;
            NativeEggProgressView eggView{};
            if(inspectNativeEggProgress(envelope,envelopeSize,PokerogueContent::kContentHash,eggView)!=NativeSaveResult::Ok ||
                eggView.generation!=17 || eggView.eggCount!=1 || eggView.vouchers[3]!=4 ||
                eggView.pity.common!=5 || eggView.pity.rare!=6 || eggView.pity.epic!=7 || eggView.pity.legendary!=8)
                return 1236;
            envelope[76]^=1;
            if(inspectNativeEggProgress(envelope,envelopeSize,PokerogueContent::kContentHash,eggView)
                !=NativeSaveResult::ChecksumMismatch || eggView.generation!=17) return 1237;
            envelope[76]^=1;char otherHash[65];std::memcpy(otherHash,PokerogueContent::kContentHash,65);
            otherHash[0]=otherHash[0]=='a'?'b':'a';
            if(inspectNativeEggProgress(envelope,envelopeSize,otherHash,eggView)!=NativeSaveResult::ContentMismatch)
                return 1238;
            envelopeSize=99;
            if(encodeNativeEggProgress(&envelopeEgg,1,balances,pityLedger,0,PokerogueContent::kContentHash,
                envelope,sizeof(envelope),envelopeSize)!=NativeSaveResult::InvalidRecord || envelopeSize!=99)
                return 1239;
        }
        {
            MemoryStorage eggStorage;char journalScratch[424]{};
            NativeEggProgressStore eggStore(eggStorage,journalScratch,sizeof(journalScratch),212);
            EggIncubationRecord egg{};egg.id=17;egg.hatchWaves=10;
            uint32_t balances[4]={1,2,3,4};EggPityState ledger{};uint32_t prepared=99;
            if(eggStore.prepare(&egg,1,balances,ledger,PokerogueContent::kContentHash,0,prepared)!=NativeSaveResult::Ok || prepared!=1)
                return 1240;
            NativeEggProgressView view{};
            if(eggStore.load(PokerogueContent::kContentHash,view,1)!=NativeSaveResult::Ok || view.generation!=1 || view.vouchers[0]!=1)
                return 1241;
            balances[0]=9;eggStorage.interrupt=true;prepared=99;
            if(eggStore.prepare(&egg,1,balances,ledger,PokerogueContent::kContentHash,1,prepared)!=NativeSaveResult::IoError || prepared!=99)
                return 1242;
            eggStorage.interrupt=false;
            if(eggStore.load(PokerogueContent::kContentHash,view,1)!=NativeSaveResult::Ok || view.vouchers[0]!=1)
                return 1243;
            if(eggStore.prepare(&egg,1,balances,ledger,PokerogueContent::kContentHash,1,prepared)!=NativeSaveResult::Ok || prepared!=2)
                return 1244;
            if(eggStore.load(PokerogueContent::kContentHash,view,1)!=NativeSaveResult::Ok || view.vouchers[0]!=1 ||
                eggStore.load(PokerogueContent::kContentHash,view,2)!=NativeSaveResult::Ok || view.vouchers[0]!=9)
                return 1245;
            if(eggStore.prepare(&egg,1,balances,ledger,PokerogueContent::kContentHash,0,prepared)!=NativeSaveResult::InvalidRecord)
                return 1246;
            if(eggStore.exportGeneration(PokerogueContent::kContentHash,1)!=NativeSaveResult::Ok ||
                inspectNativeEggProgress(eggStorage.exported,eggStorage.exportSize,PokerogueContent::kContentHash,view)
                !=NativeSaveResult::Ok || view.generation!=1 || view.vouchers[0]!=1) return 1247;
            if(eggStore.exportGeneration(PokerogueContent::kContentHash,0)!=NativeSaveResult::InvalidRecord ||
                eggStore.inspectGeneration(PokerogueContent::kContentHash,99)!=NativeSaveResult::NotFound) return 1248;
            eggStorage.slots[1][0]^=1;
            if(eggStore.load(PokerogueContent::kContentHash,view,1)!=NativeSaveResult::Ok || view.vouchers[0]!=1)
                return 1249; // Invalid checksum in the other slot must allow committed recovery.

        }
        EggTier resolvedTier=EggTier::COMMON;
        for(const auto& row:kSpeciesEggTiers)
            if(speciesEggTier(row.dex,resolvedTier)!=EggIncubationResult::Ok || resolvedTier!=row.tier) return 1214;
        resolvedTier=EggTier::LEGENDARY;
        if(speciesEggTier(65535,resolvedTier)!=EggIncubationResult::MissingSpecies || resolvedTier!=EggTier::LEGENDARY)
            return 1215;
        uint16_t candidates[sizeof(kSpeciesEggTiers)/sizeof(kSpeciesEggTiers[0])]{};
        for(const auto& policy:kEggIncubationPolicies) {
            size_t candidateCount=999,expected=0;
            if(speciesForEggTier(policy.tier,candidates,sizeof(candidates)/sizeof(candidates[0]),candidateCount)
                !=EggIncubationResult::Ok) return 1216;
            for(const auto& row:kSpeciesEggTiers) if(row.declared && row.tier==policy.tier) {
                if(expected>=candidateCount || candidates[expected]!=row.dex) return 1217;
                ++expected;
            }
            if(expected!=candidateCount || !candidateCount) return 1218;
            const auto before=candidates[0];candidateCount=999;
            if(speciesForEggTier(policy.tier,candidates,0,candidateCount)!=EggIncubationResult::OutputTooSmall ||
                candidates[0]!=before || candidateCount!=999) return 1219;
        }
        uint16_t waves=999;
        for(const auto& policy:kEggIncubationPolicies) {
            if(defaultEggIncubationWaves(1,policy.tier,waves)!=EggIncubationResult::Ok || waves!=policy.waves)
                return 1210;
            for(const auto dex:kSpecialEggIncubationSpecies)
                if(defaultEggIncubationWaves(dex,policy.tier,waves)!=EggIncubationResult::Ok || waves!=kManaphyEggHatchWaves)
                    return 1211;
        }
        waves=999;
        if(defaultEggIncubationWaves(1,static_cast<EggTier>(255),waves)!=EggIncubationResult::InvalidTier || waves!=999)
            return 1212;
        if(defaultEggIncubationWaves(65535,EggTier::COMMON,waves)!=EggIncubationResult::MissingSpecies || waves!=999)
            return 1213;
        EggIncubationRecord eggs[2]{};eggs[0].id=3;eggs[1].id=8;
        eggs[0].hatchWaves=-2147483647-1;eggs[0].timestamp=UINT64_MAX;
        eggs[0].variantTier=VariantTier::EPIC;eggs[0].isShiny=true;
        eggs[0].eggMoveIndex=3;eggs[0].overrideHiddenAbility=true;
        char encoded[60]{},repeated[60]{};size_t size=0,repeatSize=0;
        if(encodeNativeEggInventory(eggs,2,encoded,sizeof(encoded),size)!=NativeSaveResult::Ok || size!=60 ||
            encodeNativeEggInventory(eggs,2,repeated,sizeof(repeated),repeatSize)!=NativeSaveResult::Ok ||
            repeatSize!=size || std::memcmp(encoded,repeated,size)) return 1201;
        EggIncubationRecord decoded[2]{};size_t count=99;
        if(decodeNativeEggInventory(encoded,size,decoded,2,count)!=NativeSaveResult::Ok || count!=2 ||
            decoded[0].id!=3 || decoded[1].id!=8 || decoded[0].hatchWaves!=eggs[0].hatchWaves ||
            decoded[0].timestamp!=UINT64_MAX || decoded[0].variantTier!=VariantTier::EPIC ||
            !decoded[0].isShiny || decoded[0].eggMoveIndex!=3 || !decoded[0].overrideHiddenAbility) return 1202;
        decoded[0].id=55;count=99;
        if(decodeNativeEggInventory(encoded,size,decoded,1,count)!=NativeSaveResult::TooLarge ||
            decoded[0].id!=55 || count!=99) return 1203;
        encoded[35]=1;
        if(decodeNativeEggInventory(encoded,size,decoded,2,count)!=NativeSaveResult::InvalidRecord ||
            decoded[0].id!=55 || count!=99) return 1204;
        encoded[35]=0;encoded[33]=4;
        if(inspectNativeEggInventory(encoded,size,count)!=NativeSaveResult::InvalidRecord) return 1205;
        eggs[0].hatchWaves=2;eggs[1].hatchWaves=1;
        uint32_t ready[2]={99,99};count=99;
        if(lapseEggIncubation(eggs,2,ready,0,count)!=EggIncubationResult::OutputTooSmall ||
            eggs[0].hatchWaves!=2 || eggs[1].hatchWaves!=1 || count!=99 || ready[0]!=99) return 1206;
        if(lapseEggIncubation(eggs,2,ready,2,count)!=EggIncubationResult::Ok ||
            count!=1 || ready[0]!=8 || eggs[0].hatchWaves!=1 || eggs[1].hatchWaves!=0 ||
            eggs[0].timestamp!=UINT64_MAX || !eggs[0].isShiny) return 1207;
        eggs[1].id=3;size=99;
        if(encodeNativeEggInventory(eggs,2,encoded,sizeof(encoded),size)!=NativeSaveResult::InvalidRecord || size!=99)
            return 1208;
        if(encodeNativeEggInventory(nullptr,0,encoded,sizeof(encoded),size)!=NativeSaveResult::Ok || size!=12 ||
            decodeNativeEggInventory(encoded,size,nullptr,0,count)!=NativeSaveResult::Ok || count) return 1209;
    }

    const int implicitBaseForm = checkImplicitBaseFormCodec();
    if (implicitBaseForm) return implicitBaseForm;
    char digest[65];
    IntegritySha256::hashHex("abc", 3, digest);
    if (std::strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) return 1;
    IntegritySha256 streaming;
    const char* multiBlock = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    streaming.update(multiBlock, 13);
    uint8_t intermediate[32]; streaming.finish(intermediate);
    streaming.update(multiBlock + 13, 43);
    streaming.finish(intermediate); IntegritySha256::toHex(intermediate, digest);
    if (std::strcmp(digest, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") != 0) return 11;
    MemoryStorage disk; NativeRunSaveStore store(disk); NativeRunSave original{}, restored{};
    if (makeNativeRunSetupSave(123, 1, original) != NativeSaveResult::Ok) return 2;
    // v26 has Berry history but no appearance trailer. Migration keeps unknown
    // appearance instead of manufacturing a normal or shiny encounter.
    {
        char current[kNativeSaveMaxBytes]{}, legacy[kNativeSaveMaxBytes]{};
        size_t currentSize = 0;
        if (encodeNativeRunSave(original, current, sizeof(current), currentSize) != NativeSaveResult::Ok) return 10510;
        const auto* suffix = std::strstr(current, "enemyAppearances=");
        if (!suffix) return 10511;
        const size_t size = static_cast<size_t>(suffix - current);
        std::memcpy(legacy, current, size);
        auto* version = std::strstr(legacy, "saveVersion=001b");
        auto* runtime = std::strstr(legacy, "runtimeVersion=001b");
        if (!version || !runtime) return 10512;
        std::memcpy(version + 12, "001a", 4);
        std::memcpy(runtime + 15, "001a", 4);
        char checksum[65]{};
        IntegritySha256::hashHex(legacy, size, checksum);
        std::memcpy(legacy + size, "sha256=", 7);
        std::memcpy(legacy + size + 7, checksum, 64);
        legacy[size + 71] = '\n';
        NativeRunSave upgraded{};
        if (decodeNativeRunSave(legacy, size + 72, PokerogueContent::kContentHash, upgraded) != NativeSaveResult::Ok ||
            upgraded.saveVersion != kNativeSaveVersion || upgraded.runtimeVersion != kNativeSaveRuntimeVersion ||
            upgraded.enemyAppearance.resolved || upgraded.enemyAppearance.shiny || upgraded.enemyAppearance.variant)
            return 10513;
        for (const auto& member : upgraded.trainerParty)
            if (member.appearance.resolved || member.appearance.shiny || member.appearance.variant) return 10514;
        NativeRunSave invalid = original;
        invalid.enemyAppearance = {true, false, 0};
        if (validateNativeRunSave(invalid, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10515;
        if (!nativeAppearanceSaveValid({true, true, 2}) ||
            nativeAppearanceSaveValid({false, true, 0}) || nativeAppearanceSaveValid({true, false, 1}) ||
            nativeAppearanceSaveValid({true, true, 3})) return 10516;
    }
    // Exact v24 contains CRIT_BOOST, but no berry-eaten trailer.
    {
        char current[kNativeSaveMaxBytes]{}, legacy[kNativeSaveMaxBytes]{};
        size_t currentSize = 0;
        if (encodeNativeRunSave(original, current, sizeof(current), currentSize) != NativeSaveResult::Ok) return 10430;
        const auto* suffix = std::strstr(current, "berryEatenFlags=");
        if (!suffix) return 10431;
        const size_t size = static_cast<size_t>(suffix - current);
        std::memcpy(legacy, current, size);
        auto* version = std::strstr(legacy, "saveVersion=001b");
        auto* runtime = std::strstr(legacy, "runtimeVersion=001b");
        if (!version || !runtime) return 10432;
        std::memcpy(version + 12, "0018", 4);
        std::memcpy(runtime + 15, "0018", 4);
        char checksum[65]{};
        IntegritySha256::hashHex(legacy, size, checksum);
        std::memcpy(legacy + size, "sha256=", 7);
        std::memcpy(legacy + size + 7, checksum, 64);
        legacy[size + 71] = '\n';
        NativeRunSave upgraded{};
        if (decodeNativeRunSave(legacy, size + 72, PokerogueContent::kContentHash, upgraded) != NativeSaveResult::Ok ||
            upgraded.saveVersion != kNativeSaveVersion || upgraded.runtimeVersion != kNativeSaveRuntimeVersion ||
            upgraded.playerHasEatenBerry || upgraded.enemyHasEatenBerry) return 10433;
        for (const auto& member : upgraded.trainerParty) if (member.hasEatenBerry) return 10434;
    }
    // Exact v25 setup had consumption flags but no ordered histories.
    {
        char current[kNativeSaveMaxBytes]{}, legacy[kNativeSaveMaxBytes]{};
        size_t currentSize = 0;
        if (encodeNativeRunSave(original, current, sizeof(current), currentSize) != NativeSaveResult::Ok) return 10501;
        const auto* suffix = std::strstr(current, "berryHistories=");
        if (!suffix) return 10502;
        const size_t size = static_cast<size_t>(suffix - current);
        std::memcpy(legacy, current, size);
        auto* version = std::strstr(legacy, "saveVersion=001b");
        auto* runtime = std::strstr(legacy, "runtimeVersion=001b");
        if (!version || !runtime) return 10503;
        std::memcpy(version + 12, "0019", 4);
        std::memcpy(runtime + 15, "0019", 4);
        char checksum[65]{};
        IntegritySha256::hashHex(legacy, size, checksum);
        std::memcpy(legacy + size, "sha256=", 7);
        std::memcpy(legacy + size + 7, checksum, 64);
        legacy[size + 71] = '\n';
        NativeRunSave upgraded{};
        if (decodeNativeRunSave(legacy, size + 72, PokerogueContent::kContentHash, upgraded) != NativeSaveResult::Ok ||
            upgraded.saveVersion != kNativeSaveVersion || upgraded.runtimeVersion != kNativeSaveRuntimeVersion ||
            upgraded.berryHistories.resolved || upgraded.berryHistories.recordCount || upgraded.berryHistories.valueCount)
            return 10504;
    }
    // Exact v23 setup: no persistent suffix when its count is zero, and no
    // v24 critical-tag trailer. Upgrade must initialize every critical tag absent.
    {
        char current[kNativeSaveMaxBytes]{}, legacy[kNativeSaveMaxBytes]{};
        size_t currentSize = 0;
        if (encodeNativeRunSave(original, current, sizeof(current), currentSize) != NativeSaveResult::Ok) return 10370;
        const auto* suffix = std::strstr(current, "persistentModifierCount=");
        if (!suffix) return 10371;
        const size_t size = static_cast<size_t>(suffix - current);
        std::memcpy(legacy, current, size);
        auto* version = std::strstr(legacy, "saveVersion=001b");
        auto* runtime = std::strstr(legacy, "runtimeVersion=001b");
        if (!version || !runtime) return 10372;
        std::memcpy(version + 12, "0017", 4);
        std::memcpy(runtime + 15, "0017", 4);
        char checksum[65]{};
        IntegritySha256::hashHex(legacy, size, checksum);
        std::memcpy(legacy + size, "sha256=", 7);
        std::memcpy(legacy + size + 7, checksum, 64);
        legacy[size + 71] = '\n';
        NativeRunSave upgraded{};
        if (decodeNativeRunSave(legacy, size + 72, PokerogueContent::kContentHash, upgraded) != NativeSaveResult::Ok ||
            upgraded.saveVersion != kNativeSaveVersion || upgraded.runtimeVersion != kNativeSaveRuntimeVersion ||
            upgraded.playerBerryCriticalBoostStages || upgraded.enemyBerryCriticalBoostStages) return 10373;
        for (const auto& member : upgraded.trainerParty) if (member.berryCriticalBoostStages) return 10374;
    }

    uint16_t nonProfileStarterDex = 0;
    for (std::size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
        const auto& species = PokerogueContent::kSpecies[i];
        if (species.starterEligible && !species.freshProfileStarter) {
            nonProfileStarterDex = species.dex;
            break;
        }
    }
    // Codec validates canonical eligibility. Runtime regressions 618–620
    // separately require caught-profile authorization for non-default starters.
    NativeRunSave unlockedStarter{};
    if (!nonProfileStarterDex || makeNativeRunSetupSave(123, nonProfileStarterDex, unlockedStarter) !=
            NativeSaveResult::Ok || unlockedStarter.starterDex != nonProfileStarterDex ||
        validateNativeRunSave(unlockedStarter, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 12;
    uint16_t ineligibleStarterDex = 0;
    for (const auto& species : PokerogueContent::kSpecies)
        if (!species.starterEligible) { ineligibleStarterDex = species.dex; break; }
    NativeRunSave invalidStarter{};
    if (!ineligibleStarterDex || makeNativeRunSetupSave(123, ineligibleStarterDex, invalidStarter) !=
            NativeSaveResult::InvalidRecord) return 115;
    if (store.save(original) != NativeSaveResult::Ok || store.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok || restored.seed != 123) return 3;
    original.seed = 456; disk.interrupt = true;
    if (store.save(original) != NativeSaveResult::IoError) return 4;
    if (store.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok || restored.seed != 123) return 5;
    disk.interrupt = false;
    if (store.save(original) != NativeSaveResult::Ok || store.exportLatest(PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 6;
    MemoryStorage other; NativeRunSaveStore imported(other);
    other.writeExport(disk.exported, disk.exportSize);
    if (imported.importExport(PokerogueContent::kContentHash) != NativeSaveResult::Ok || imported.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok || restored.seed != 456) return 7;
    other.exported[other.exportSize / 2] ^= 1;
    if (imported.importExport(PokerogueContent::kContentHash) == NativeSaveResult::Ok) return 8;
    if (imported.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok || restored.seed != 456) return 9;
    char wrongHash[65]; std::memcpy(wrongHash, PokerogueContent::kContentHash, 65); wrongHash[0] = wrongHash[0] == '0' ? '1' : '0';
    if (imported.load(wrongHash, restored) != NativeSaveResult::ContentMismatch) return 10;
    {
        MemoryStorage deleteDisk;
        NativeRunSaveStore deleteStore(deleteDisk);
        NativeRunSave toSave{}, toLoad{};
        if (makeNativeRunSetupSave(789, 1, toSave) != NativeSaveResult::Ok) return 13000;
        if (deleteStore.save(toSave) != NativeSaveResult::Ok) return 13001;
        if (deleteStore.load(PokerogueContent::kContentHash, toLoad) != NativeSaveResult::Ok) return 13002;
        if (deleteStore.deleteSave() != NativeSaveResult::Ok) return 13003;
        if (deleteStore.load(PokerogueContent::kContentHash, toLoad) != NativeSaveResult::NotFound) return 13004;
    }
    NativeRunSave trainerSave = original;
    trainerSave.wave = 5;
    trainerSave.stage = NativeSaveStage::BattleActive;
    trainerSave.battleTurn = 1;
    trainerSave.playerHp = 20;
    trainerSave.encounterDex = 19;
    trainerSave.enemyHp = 12;
    trainerSave.playerMoveCount = trainerSave.enemyMoveCount = 1;
    trainerSave.playerMoveIds[0] = trainerSave.enemyMoveIds[0] = 33;
    trainerSave.playerPp[0] = trainerSave.enemyPp[0] = 25;
    trainerSave.playerStatStages[0] = -6;
    trainerSave.enemyStatStages[4] = 6;
    trainerSave.enemySwitchCounter = 2;
    trainerSave.trainerTypeId = 63;
    trainerSave.trainerPartyCount = 2;
    trainerSave.activeTrainerMember = 0;
    trainerSave.trainerParty[0] = {19, 12, 1, {33, 0, 0, 0}, {25, 0, 0, 0}};
    trainerSave.trainerParty[1] = {504, 9, 1, {33, 0, 0, 0}, {17, 0, 0, 0}};
    trainerSave.trainerParty[0].statStages[4] = 6;
    trainerSave.trainerParty[1].statStages[5] = -2;
    trainerSave.weatherType = 2;
    trainerSave.weatherTurnsLeft = 3;
    trainerSave.weatherMaxDuration = 5;
    char partyBytes[kNativeSaveMaxBytes]{};
    size_t partySize = 0;
    if (encodeNativeRunSave(trainerSave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.weatherType != 2 || restored.weatherTurnsLeft != 3 || restored.weatherMaxDuration != 5 ||
        restored.playerStatStages[0] != -6 || restored.enemyStatStages[4] != 6 ||
        restored.trainerParty[1].statStages[5] != -2 || restored.enemySwitchCounter != 2 || restored.trainerPartyCount != 2 || restored.trainerParty[1].hp != 9 ||
        restored.trainerParty[1].pp[0] != 17 || restored.activeTrainerMember != 0) return 13;
    {
        auto appearanceSave = trainerSave;
        appearanceSave.enemyAppearance = {true, true, 2};
        appearanceSave.trainerParty[0].appearance = appearanceSave.enemyAppearance;
        appearanceSave.trainerParty[1].appearance = {true, false, 0};
        NativeRunSave decodedAppearance{};
        if (encodeNativeRunSave(appearanceSave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
            decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, decodedAppearance) != NativeSaveResult::Ok ||
            !decodedAppearance.enemyAppearance.resolved || !decodedAppearance.enemyAppearance.shiny ||
            decodedAppearance.enemyAppearance.variant != 2 || !decodedAppearance.trainerParty[1].appearance.resolved ||
            decodedAppearance.trainerParty[1].appearance.shiny || decodedAppearance.trainerParty[1].appearance.variant)
            return 10517;
        appearanceSave.trainerParty[0].appearance.variant = 1;
        if (validateNativeRunSave(appearanceSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10518;
        appearanceSave = trainerSave;
        appearanceSave.trainerParty[5].appearance = {true, false, 0};
        if (validateNativeRunSave(appearanceSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10519;
    }
    {
        auto bossSave = trainerSave;
        bossSave.wave = 10;
        bossSave.trainerTypeId = 0;
        bossSave.trainerPartyCount = 0;
        bossSave.activeTrainerMember = 0xff;
        bossSave.enemySwitchCounter = 0;
        for (auto& member : bossSave.trainerParty) member = {};
        bossSave.enemyBoss = {2, 0, false, false};
        char bossBytes[kNativeSaveMaxBytes]{};
        size_t bossSize = 0;
        NativeRunSave decodedBoss{};
        if (encodeNativeRunSave(bossSave, bossBytes, sizeof(bossBytes), bossSize) != NativeSaveResult::Ok ||
            decodeNativeRunSave(bossBytes, bossSize, PokerogueContent::kContentHash, decodedBoss) != NativeSaveResult::Ok ||
            decodedBoss.enemyBoss.segmentCount != 2 || decodedBoss.enemyBoss.segmentIndex != 0) return 10130;
        bossSave.enemyBoss.segmentIndex = 2;
        if (validateNativeRunSave(bossSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10131;
        bossSave.enemyBoss = {0, 1, false, false};
        if (validateNativeRunSave(bossSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10132;
        bossSave.enemyBoss = {2, 0, true, false};
        if (validateNativeRunSave(bossSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10133;
    }
    // v21 serializes Alea state losslessly as exact 32-bit fractions, without clock metadata.
    {
        auto randomSave = trainerSave;
        PokerogueRngAdapter source;
        const uint16_t root[] = {'s', 'h', 'i', 'e', 'l', 'd'};
        source.sow(root, 6);
        for (unsigned i = 0; i < 17; ++i) source.randSeedUint32();
        randomSave.globalRngResolved = true;
        randomSave.globalRng = source.state();
        char randomBytes[kNativeSaveMaxBytes]{};
        size_t randomSize = 0;
        NativeRunSave decoded{};
        if (encodeNativeRunSave(randomSave, randomBytes, sizeof(randomBytes), randomSize) != NativeSaveResult::Ok ||
            decodeNativeRunSave(randomBytes, randomSize, PokerogueContent::kContentHash, decoded) != NativeSaveResult::Ok ||
            !decoded.globalRngResolved || decoded.globalRng.carry != randomSave.globalRng.carry ||
            decoded.globalRng.s0 != randomSave.globalRng.s0 || decoded.globalRng.s1 != randomSave.globalRng.s1 ||
            decoded.globalRng.s2 != randomSave.globalRng.s2) return 10260;
        size_t repeatedSize = 0;
        if (encodeNativeRunSave(randomSave, partyBytes, sizeof(partyBytes), repeatedSize) != NativeSaveResult::Ok ||
            repeatedSize != randomSize || std::memcmp(partyBytes, randomBytes, randomSize)) return 10268;
        PokerogueRngAdapter replay;
        replay.restore(decoded.globalRng);
        for (unsigned i = 0; i < 32; ++i) if (source.randSeedUint32() != replay.randSeedUint32()) return 10261;
        auto invalid = randomSave;
        invalid.globalRng.s0 = 1.0;
        if (validateNativeRunSave(invalid, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10262;
        invalid = randomSave;
        invalid.globalRng.carry = 0.5;
        if (validateNativeRunSave(invalid, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10263;
        invalid = randomSave;
        invalid.globalRngResolved = false;
        if (validateNativeRunSave(invalid, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10264;
        const auto* suffix = std::strstr(randomBytes, "globalRng=");
        if (!suffix) return 10265;
        const size_t payloadSize = static_cast<size_t>(suffix - randomBytes);
        char legacy[kNativeSaveMaxBytes]{};
        std::memcpy(legacy, randomBytes, payloadSize);
        char* version = std::strstr(legacy, "saveVersion=001b");
        char* runtime = std::strstr(legacy, "runtimeVersion=001b");
        if (!version || !runtime) return 10266;
        std::memcpy(version + 12, "0014", 4);
        std::memcpy(runtime + 15, "0014", 4);
        char checksum[65]{};
        IntegritySha256::hashHex(legacy, payloadSize, checksum);
        std::memcpy(legacy + payloadSize, "sha256=", 7);
        std::memcpy(legacy + payloadSize + 7, checksum, 64);
        legacy[payloadSize + 71] = '\n';
        if (decodeNativeRunSave(legacy, payloadSize + 72, PokerogueContent::kContentHash, decoded) != NativeSaveResult::Ok ||
            decoded.saveVersion != kNativeSaveVersion || decoded.globalRngResolved || decoded.globalRng.carry ||
            decoded.globalRng.s0 || decoded.globalRng.s1 || decoded.globalRng.s2) return 10267;
        // Exact v21 payload keeps global RNG but omits the v22 double suffix.
        suffix = std::strstr(randomBytes, "doubleBattle=");
        if (!suffix) return 10290;
        const size_t v21Size = static_cast<size_t>(suffix - randomBytes);
        std::memset(legacy, 0, sizeof(legacy));
        std::memcpy(legacy, randomBytes, v21Size);
        version = std::strstr(legacy, "saveVersion=001b");
        runtime = std::strstr(legacy, "runtimeVersion=001b");
        if (!version || !runtime) return 10291;
        std::memcpy(version + 12, "0015", 4);
        std::memcpy(runtime + 15, "0015", 4);
        IntegritySha256::hashHex(legacy, v21Size, checksum);
        std::memcpy(legacy + v21Size, "sha256=", 7);
        std::memcpy(legacy + v21Size + 7, checksum, 64);
        legacy[v21Size + 71] = '\n';
        if (decodeNativeRunSave(legacy, v21Size + 72, PokerogueContent::kContentHash, decoded) != NativeSaveResult::Ok ||
            decoded.saveVersion != kNativeSaveVersion || decoded.doubleBattle || !decoded.globalRngResolved ||
            decoded.globalRng.s0 != randomSave.globalRng.s0 || decoded.globalRng.s1 != randomSave.globalRng.s1 ||
            decoded.globalRng.s2 != randomSave.globalRng.s2 || decoded.globalRng.carry != randomSave.globalRng.carry)
            return 10292;
        invalid = randomSave;
        invalid.secondEnemy.level = 1;
        if (validateNativeRunSave(invalid, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10293;
    }
    // A defeat needs no living enemy: final recoil can knock out both complete parties.
    {
        auto lost = trainerSave;
        lost.stage = NativeSaveStage::BattleLost;
        lost.playerHp = lost.enemyHp = 0;
        for (uint8_t i = 0; i < lost.trainerPartyCount; ++i) lost.trainerParty[i].hp = 0;
        char lostBytes[kNativeSaveMaxBytes]{};
        size_t lostSize = 0;
        NativeRunSave decoded{};
        if (encodeNativeRunSave(lost, lostBytes, sizeof(lostBytes), lostSize) != NativeSaveResult::Ok ||
            decodeNativeRunSave(lostBytes, lostSize, PokerogueContent::kContentHash, decoded) != NativeSaveResult::Ok ||
            decoded.stage != NativeSaveStage::BattleLost || decoded.playerHp || decoded.enemyHp ||
            decoded.trainerParty[1].hp) return 10320;
        lost.stage = NativeSaveStage::BattleActive;
        if (validateNativeRunSave(lost, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10321;
        lost.stage = NativeSaveStage::BattleWon;
        if (validateNativeRunSave(lost, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10322;
    }
    NativeRunSave statusSave = trainerSave;
    statusSave.playerStatus.present = true;
    statusSave.playerStatus.effect = PokemonStatusEffect::Burn;
    statusSave.enemyStatus.present = true;
    statusSave.enemyStatus.effect = PokemonStatusEffect::Sleep;
    statusSave.enemyStatus.hasSleepTurnsRemaining = true;
    statusSave.enemyStatus.sleepTurnsRemaining = 2;
    statusSave.trainerParty[0].status = statusSave.enemyStatus;
    statusSave.trainerParty[1].status.present = true;
    statusSave.trainerParty[1].status.effect = PokemonStatusEffect::Toxic;
    statusSave.trainerParty[1].status.toxicTurnCount = 5;
    if (encodeNativeRunSave(statusSave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        !restored.playerStatus.present || restored.playerStatus.effect != PokemonStatusEffect::Burn ||
        !restored.enemyStatus.hasSleepTurnsRemaining || restored.enemyStatus.sleepTurnsRemaining != 2 ||
        restored.trainerParty[1].status.toxicTurnCount != 5) return 9004;
    statusSave.trainerParty[0].status.sleepTurnsRemaining = 1;
    if (validateNativeRunSave(statusSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 9005;
    NativeRunSave confusionSave = trainerSave;
    confusionSave.playerConfusion = {3, true, 109, true, 0, true};
    confusionSave.enemyConfusion = {2, true, 93, true, 0xffffffffu, true};
    confusionSave.trainerParty[confusionSave.activeTrainerMember].confusion = confusionSave.enemyConfusion;
    confusionSave.trainerParty[1].confusion = {5, true, 0, true, 42, true};
    if (encodeNativeRunSave(confusionSave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != kNativeSaveVersion || restored.playerConfusion.turns != 3 ||
        restored.enemyConfusion.turns != 2 || restored.trainerParty[1].confusion.turns != 5 ||
        !restored.playerConfusion.sourceMoveResolved || restored.playerConfusion.sourceMoveId != 109 ||
        !restored.enemyConfusion.sourceMoveResolved || restored.enemyConfusion.sourceMoveId != 93 ||
        !restored.trainerParty[1].confusion.sourceMoveResolved || restored.trainerParty[1].confusion.sourceMoveId ||
        !restored.playerConfusion.sourcePokemonResolved || restored.playerConfusion.sourcePokemonId ||
        !restored.enemyConfusion.sourcePokemonResolved || restored.enemyConfusion.sourcePokemonId != 0xffffffffu ||
        restored.trainerParty[1].confusion.sourcePokemonId != 42) return 9180;
    {
        auto survivalSave = confusionSave;
        survivalSave.enemySturdyTag = true;
        survivalSave.trainerParty[survivalSave.activeTrainerMember].sturdyTag = true;
        survivalSave.trainerParty[1].sturdyTag = true;
        char survivalBytes[kNativeSaveMaxBytes]{};
        size_t survivalSize = 0;
        NativeRunSave survivalDecoded{};
        if (encodeNativeRunSave(survivalSave, survivalBytes, sizeof(survivalBytes), survivalSize) != NativeSaveResult::Ok ||
            decodeNativeRunSave(survivalBytes, survivalSize, PokerogueContent::kContentHash, survivalDecoded) != NativeSaveResult::Ok ||
            !survivalDecoded.enemySturdyTag || !survivalDecoded.trainerParty[0].sturdyTag ||
            !survivalDecoded.trainerParty[1].sturdyTag || survivalDecoded.enemyConfusion.turns != 2) return 10090;
        char* tagField = std::strstr(survivalBytes, "enemySturdy=");
        if (!tagField) return 10096;
        tagField[12] = '2';
        IntegritySha256::hashHex(survivalBytes, survivalSize - 72, digest);
        std::memcpy(survivalBytes + survivalSize - 65, digest, 64);
        if (decodeNativeRunSave(survivalBytes, survivalSize, PokerogueContent::kContentHash, survivalDecoded) !=
                NativeSaveResult::InvalidFormat || !survivalDecoded.enemySturdyTag) return 10097;
        auto invalid = survivalSave;
        invalid.trainerParty[0].sturdyTag = false;
        if (validateNativeRunSave(invalid, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10091;
        invalid = survivalSave;
        invalid.trainerParty[5].sturdyTag = true;
        if (validateNativeRunSave(invalid, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10092;
        // Exact v18 payload omits the new suffix; checksum is recalculated.
        char legacy[kNativeSaveMaxBytes]{};
        const char* suffix = std::strstr(partyBytes, "enemySturdy=");
        if (!suffix) return 10093;
        const size_t payloadSize = static_cast<size_t>(suffix - partyBytes);
        std::memcpy(legacy, partyBytes, payloadSize);
        char* version = std::strstr(legacy, "saveVersion=001b");
        char* runtime = std::strstr(legacy, "runtimeVersion=001b");
        if (!version || !runtime) return 10094;
        std::memcpy(version + 12, "0012", 4);
        std::memcpy(runtime + 15, "0012", 4);
        IntegritySha256::hashHex(legacy, payloadSize, digest);
        std::memcpy(legacy + payloadSize, "sha256=", 7);
        std::memcpy(legacy + payloadSize + 7, digest, 64);
        legacy[payloadSize + 71] = '\n';
        if (decodeNativeRunSave(legacy, payloadSize + 72, PokerogueContent::kContentHash, survivalDecoded) != NativeSaveResult::Ok ||
            survivalDecoded.saveVersion != kNativeSaveVersion || survivalDecoded.enemySturdyTag ||
            survivalDecoded.trainerParty[0].sturdyTag || survivalDecoded.trainerParty[1].sturdyTag ||
            survivalDecoded.enemyConfusion.sourcePokemonId != 0xffffffffu) return 10095;
        const char* bossSuffix = std::strstr(partyBytes, "enemyBoss=");
        if (!bossSuffix) return 10134;
        const size_t v19Size = static_cast<size_t>(bossSuffix - partyBytes);
        std::memset(legacy, 0, sizeof(legacy));
        std::memcpy(legacy, partyBytes, v19Size);
        version = std::strstr(legacy, "saveVersion=001b");
        runtime = std::strstr(legacy, "runtimeVersion=001b");
        if (!version || !runtime) return 10135;
        std::memcpy(version + 12, "0013", 4);
        std::memcpy(runtime + 15, "0013", 4);
        IntegritySha256::hashHex(legacy, v19Size, digest);
        std::memcpy(legacy + v19Size, "sha256=", 7);
        std::memcpy(legacy + v19Size + 7, digest, 64);
        legacy[v19Size + 71] = '\n';
        if (decodeNativeRunSave(legacy, v19Size + 72, PokerogueContent::kContentHash, survivalDecoded) != NativeSaveResult::Ok ||
            survivalDecoded.enemyBoss.segmentCount || survivalDecoded.enemyBoss.segmentIndex ||
            survivalDecoded.enemyConfusion.sourcePokemonId != 0xffffffffu) return 10136;

    }
    {
        // Build the exact v16 layout by removing only v17 source metadata.
        char legacyBytes[kNativeSaveMaxBytes]{};
        const char* sourceStart = std::strstr(partyBytes, "playerConfusionSource=");
        if (!sourceStart || confusionSave.playerPartyCount) return 9511;
        const size_t legacyPayload = static_cast<size_t>(sourceStart - partyBytes);
        std::memcpy(legacyBytes, partyBytes, legacyPayload);
        for (size_t n = 0; n < legacyPayload; ++n) {
            if (n + 16 <= legacyPayload && std::memcmp(legacyBytes + n, "saveVersion=001b", 16) == 0)
                std::memcpy(legacyBytes + n + 12, "0010", 4);
            if (n + 19 <= legacyPayload && std::memcmp(legacyBytes + n, "runtimeVersion=001b", 19) == 0)
                std::memcpy(legacyBytes + n + 15, "0010", 4);
        }
        IntegritySha256::hashHex(legacyBytes, legacyPayload, digest);
        std::memcpy(legacyBytes + legacyPayload, "sha256=", 7);
        std::memcpy(legacyBytes + legacyPayload + 7, digest, 64);
        legacyBytes[legacyPayload + 71] = '\n';
        if (decodeNativeRunSave(legacyBytes, legacyPayload + 72, PokerogueContent::kContentHash, restored) !=
                NativeSaveResult::Ok || restored.saveVersion != kNativeSaveVersion ||
            restored.playerConfusion.turns != 3 || restored.enemyConfusion.turns != 2 ||
            restored.playerConfusion.sourceMoveResolved || restored.playerConfusion.sourceMoveId ||
            restored.enemyConfusion.sourceMoveResolved || restored.enemyConfusion.sourceMoveId ||
            restored.trainerParty[1].confusion.sourceMoveResolved) return 9512;
        const char* actorStart = std::strstr(partyBytes, "playerConfusionActor=");
        if (!actorStart) return 9521;
        const size_t v17Payload = static_cast<size_t>(actorStart - partyBytes);
        std::memcpy(legacyBytes, partyBytes, v17Payload);
        for (size_t n = 0; n < v17Payload; ++n) {
            if (n + 16 <= v17Payload && std::memcmp(legacyBytes + n, "saveVersion=001b", 16) == 0)
                std::memcpy(legacyBytes + n + 12, "0011", 4);
            if (n + 19 <= v17Payload && std::memcmp(legacyBytes + n, "runtimeVersion=001b", 19) == 0)
                std::memcpy(legacyBytes + n + 15, "0011", 4);
        }
        IntegritySha256::hashHex(legacyBytes, v17Payload, digest);
        std::memcpy(legacyBytes + v17Payload, "sha256=", 7);
        std::memcpy(legacyBytes + v17Payload + 7, digest, 64);
        legacyBytes[v17Payload + 71] = '\n';
        if (decodeNativeRunSave(legacyBytes, v17Payload + 72, PokerogueContent::kContentHash, restored) !=
                NativeSaveResult::Ok || restored.saveVersion != kNativeSaveVersion ||
            restored.playerConfusion.sourceMoveId != 109 || !restored.playerConfusion.sourceMoveResolved ||
            restored.enemyConfusion.sourceMoveId != 93 || restored.enemyConfusion.sourcePokemonResolved ||
            restored.enemyConfusion.sourcePokemonId) return 9522;
        auto invalidActor = confusionSave;
        invalidActor.enemyConfusion.sourcePokemonId = 42;
        if (validateNativeRunSave(invalidActor, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
            return 9523;
        invalidActor = confusionSave;
        invalidActor.enemyConfusion.sourcePokemonResolved = false;
        invalidActor.trainerParty[0].confusion = invalidActor.enemyConfusion;
        if (validateNativeRunSave(invalidActor, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
            return 9524;
        auto invalidSource = confusionSave;
        invalidSource.enemyConfusion.sourceMoveId = 60;
        if (validateNativeRunSave(invalidSource, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
            return 9513;
        invalidSource = confusionSave;
        invalidSource.playerConfusion.sourceMoveResolved = false;
        if (validateNativeRunSave(invalidSource, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
            return 9514;
        invalidSource = confusionSave;
        invalidSource.playerConfusion.sourceMoveId = 65535;
        if (validateNativeRunSave(invalidSource, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
            return 9515;
    }
    // A valid checksum does not authorize trailing payload fields. Check
    // completion after v16 confusion fields, preserving output on rejection.
    {
        constexpr size_t digestLineBytes = 7 + 64 + 1;
        const char extra[] = "unexpected=00000000\n";
        const size_t extraBytes = sizeof(extra) - 1;
        const size_t payloadEnd = partySize - digestLineBytes;
        if (partySize + extraBytes > sizeof(partyBytes)) return 9500;
        for (size_t n = partySize; n > payloadEnd; --n)
            partyBytes[n + extraBytes - 1] = partyBytes[n - 1];
        std::memcpy(partyBytes + payloadEnd, extra, extraBytes);
        IntegritySha256::hashHex(partyBytes, payloadEnd + extraBytes, digest);
        std::memcpy(partyBytes + payloadEnd + extraBytes + 7, digest, 64);
        restored.seed = 0x12345678u;
        if (decodeNativeRunSave(partyBytes, partySize + extraBytes, PokerogueContent::kContentHash, restored) !=
                NativeSaveResult::InvalidFormat || restored.seed != 0x12345678u) return 9501;
        if (encodeNativeRunSave(confusionSave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
            decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
            restored.enemyConfusion.turns != 2) return 9502;
    }
    confusionSave.enemyConfusion.turns = 4;
    if (validateNativeRunSave(confusionSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 9181;
    confusionSave.enemyConfusion = confusionSave.trainerParty[0].confusion;
    confusionSave.playerConfusion.present = false;
    if (validateNativeRunSave(confusionSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 9182;
    NativeRunSave roomSave = trainerSave;
    roomSave.trickRoomTurnsLeft = 3;
    roomSave.trickRoomMaxDuration = 5;
    roomSave.trickRoomSourceMoveId = 433;
    roomSave.trickRoomSourcePokemonId = 123;
    char roomBytes[kNativeSaveMaxBytes]{};
    size_t roomSize = 0;
    if (encodeNativeRunSave(roomSave, roomBytes, sizeof(roomBytes), roomSize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(roomBytes, roomSize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.trickRoomTurnsLeft != 3 || restored.trickRoomMaxDuration != 5 ||
        restored.trickRoomSourceMoveId != 433 || restored.trickRoomSourcePokemonId != 123) return 38;
    roomSave.trickRoomSourceMoveId = 33;
    if (validateNativeRunSave(roomSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 39;
    char versionSevenBytes[kNativeSaveMaxBytes]{};
    std::memcpy(versionSevenBytes, roomBytes, roomSize);
    size_t versionSevenSize = 0;
    for (size_t n = 0; n < roomSize; ++n) {
        if (n + 18 <= roomSize && std::memcmp(versionSevenBytes + n, "trickRoomTurnsLeft=", 18) == 0) {
            versionSevenSize = n; break;
        }
        if (n + 16 <= roomSize && std::memcmp(versionSevenBytes + n, "saveVersion=", 12) == 0)
            std::memcpy(versionSevenBytes + n + 12, "0007", 4);
        if (n + 19 <= roomSize && std::memcmp(versionSevenBytes + n, "runtimeVersion=", 15) == 0)
            std::memcpy(versionSevenBytes + n + 15, "0007", 4);
    }
    if (!versionSevenSize) return 40;
    IntegritySha256::hashHex(versionSevenBytes, versionSevenSize, digest);
    std::memcpy(versionSevenBytes + versionSevenSize, "sha256=", 7);
    std::memcpy(versionSevenBytes + versionSevenSize + 7, digest, 64);
    versionSevenBytes[versionSevenSize + 71] = '\n';
    if (decodeNativeRunSave(versionSevenBytes, versionSevenSize + 72, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != kNativeSaveVersion || restored.weatherType != 2 ||
        restored.trickRoomTurnsLeft || restored.trickRoomMaxDuration ||
        restored.trickRoomSourceMoveId || restored.trickRoomSourcePokemonId) return 41;
    NativeRunSave invalidWeather = trainerSave;
    invalidWeather.weatherTurnsLeft = 6;
    if (validateNativeRunSave(invalidWeather, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 34;
    invalidWeather = trainerSave;
    invalidWeather.weatherType = 7; // Immutable weather cannot carry a timer.
    if (validateNativeRunSave(invalidWeather, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 35;
    char versionSixBytes[kNativeSaveMaxBytes]{};
    std::memcpy(versionSixBytes, partyBytes, partySize);
    size_t versionSixSize = 0;
    for (size_t n = 0; n < partySize; ++n) {
        if (n + 12 <= partySize && std::memcmp(versionSixBytes + n, "weatherType=", 12) == 0) {
            versionSixSize = n; break;
        }
        if (n + 16 <= partySize && std::memcmp(versionSixBytes + n, "saveVersion=", 12) == 0)
            std::memcpy(versionSixBytes + n + 12, "0006", 4);
        if (n + 19 <= partySize && std::memcmp(versionSixBytes + n, "runtimeVersion=", 15) == 0)
            std::memcpy(versionSixBytes + n + 15, "0006", 4);
    }
    if (!versionSixSize) return 36;
    IntegritySha256::hashHex(versionSixBytes, versionSixSize, digest);
    std::memcpy(versionSixBytes + versionSixSize, "sha256=", 7);
    std::memcpy(versionSixBytes + versionSixSize + 7, digest, 64);
    versionSixBytes[versionSixSize + 71] = '\n';
    if (decodeNativeRunSave(versionSixBytes, versionSixSize + 72, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != kNativeSaveVersion || restored.weatherType || restored.weatherTurnsLeft ||
        restored.weatherMaxDuration || restored.playerStatStages[0] != -6) return 37;
    char legacyTrainerBytes[kNativeSaveMaxBytes]{};
    size_t legacyTrainerSize = 0;
    const size_t trainerBodyEnd = partySize - 72;
    for (size_t n = 0; n < trainerBodyEnd;) {
        if (n + 13 <= trainerBodyEnd && std::memcmp(partyBytes + n, "playerStages=", 13) == 0) break;
        if (n + 28 <= trainerBodyEnd &&
            std::memcmp(partyBytes + n, "enemySwitchCounter=", 19) == 0) { n += 28; continue; }
        legacyTrainerBytes[legacyTrainerSize++] = partyBytes[n++];
    }
    for (size_t n = 0; n < legacyTrainerSize; ++n) {
        if (n + 16 <= legacyTrainerSize && std::memcmp(legacyTrainerBytes + n, "saveVersion=", 12) == 0)
            std::memcpy(legacyTrainerBytes + n + 12, "0004", 4);
        if (n + 19 <= legacyTrainerSize && std::memcmp(legacyTrainerBytes + n, "runtimeVersion=", 15) == 0)
            std::memcpy(legacyTrainerBytes + n + 15, "0004", 4);
    }
    IntegritySha256::hashHex(legacyTrainerBytes, legacyTrainerSize, digest);
    std::memcpy(legacyTrainerBytes + legacyTrainerSize, "sha256=", 7);
    std::memcpy(legacyTrainerBytes + legacyTrainerSize + 7, digest, 64);
    legacyTrainerBytes[legacyTrainerSize + 71] = '\n';
    if (decodeNativeRunSave(legacyTrainerBytes, legacyTrainerSize + 72,
            PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != kNativeSaveVersion || restored.enemySwitchCounter ||
        restored.trainerPartyCount != 2 || restored.trainerParty[1].pp[0] != 17) return 22;
    trainerSave.trainerPartyCount = 6;
    for (uint8_t member = 2; member < 6; ++member)
        trainerSave.trainerParty[member] = trainerSave.trainerParty[1];
    if (encodeNativeRunSave(trainerSave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.trainerPartyCount != 6 || restored.trainerParty[5].pp[0] != 17) return 21;
    trainerSave.trainerPartyCount = 2;
    for (uint8_t member = 2; member < 6; ++member) trainerSave.trainerParty[member] = {};
    trainerSave.activeTrainerMember = 2;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 14;
    trainerSave.activeTrainerMember = 0;
    trainerSave.trainerParty[0].hp = 11;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 15;
    trainerSave.trainerParty[0].hp = trainerSave.enemyHp = 0;
    trainerSave.stage = NativeSaveStage::BattleWon;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 16;
    // Per-Pokemon victory must survive the codec with the trainer reserve alive.
    size_t intermediateSize = 0;
    if (encodeNativeRunSave(trainerSave, partyBytes, sizeof(partyBytes), intermediateSize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, intermediateSize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.trainerParty[0].hp || !restored.trainerParty[1].hp ||
        restored.stage != NativeSaveStage::BattleWon) return 10160;
    trainerSave.trainerParty[1].hp = 0;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 17;
    if (encodeNativeRunSave(original, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok) return 18;
    size_t legacyBodySize = 0;
    for (size_t n = 0; n < partySize; ++n) {
        if (n + 19 <= partySize && std::memcmp(partyBytes + n, "enemySwitchCounter=", 19) == 0) {
            legacyBodySize = n; break;
        }
        if (n + 16 <= partySize && std::memcmp(partyBytes + n, "saveVersion=", 12) == 0)
            std::memcpy(partyBytes + n + 12, "0003", 4);
        if (n + 19 <= partySize && std::memcmp(partyBytes + n, "runtimeVersion=", 15) == 0)
            std::memcpy(partyBytes + n + 15, "0003", 4);
    }
    if (!legacyBodySize) return 19;
    IntegritySha256::hashHex(partyBytes, legacyBodySize, digest);
    std::memcpy(partyBytes + legacyBodySize, "sha256=", 7);
    std::memcpy(partyBytes + legacyBodySize + 7, digest, 64);
    partyBytes[legacyBodySize + 71] = '\n';
    if (decodeNativeRunSave(partyBytes, legacyBodySize + 72, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != kNativeSaveVersion || restored.runtimeVersion != kNativeSaveRuntimeVersion ||
        restored.seed != original.seed || restored.trainerPartyCount || restored.activeTrainerMember != 0xFF) return 20;
    NativeRunSave inventorySave = trainerSave;
    inventorySave.pokeballCounts[0] = 0;
    inventorySave.pokeballCounts[1] = 3;
    inventorySave.pokeballCounts[2] = 99;
    inventorySave.pokeballCounts[3] = 1;
    inventorySave.pokeballCounts[4] = 2;
    if (encodeNativeRunSave(inventorySave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok)
        return 40;
    for (uint8_t ball = 0; ball < 5; ++ball)
        if (inventorySave.pokeballCounts[ball] != restored.pokeballCounts[ball]) return 41;
    inventorySave.pokeballCounts[2] = 100;
    if (validateNativeRunSave(inventorySave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 42;
    // v8 has Trick Room but no inventory; its supported production stock was initial.
    char versionEightBytes[kNativeSaveMaxBytes]{};
    std::memcpy(versionEightBytes, roomBytes, roomSize);
    size_t versionEightSize = 0;
    for (size_t n = 0; n < roomSize; ++n) {
        if (n + 14 <= roomSize && std::memcmp(versionEightBytes + n, "pokeballCount=", 14) == 0) {
            versionEightSize = n; break;
        }
        if (n + 16 <= roomSize && std::memcmp(versionEightBytes + n, "saveVersion=", 12) == 0)
            std::memcpy(versionEightBytes + n + 12, "0008", 4);
        if (n + 19 <= roomSize && std::memcmp(versionEightBytes + n, "runtimeVersion=", 15) == 0)
            std::memcpy(versionEightBytes + n + 15, "0008", 4);
    }
    if (!versionEightSize) return 43;
    IntegritySha256::hashHex(versionEightBytes, versionEightSize, digest);
    std::memcpy(versionEightBytes + versionEightSize, "sha256=", 7);
    std::memcpy(versionEightBytes + versionEightSize + 7, digest, 64);
    versionEightBytes[versionEightSize + 71] = '\n';
    if (decodeNativeRunSave(versionEightBytes, versionEightSize + 72, PokerogueContent::kContentHash,
            restored) != NativeSaveResult::Ok || restored.saveVersion != kNativeSaveVersion ||
        restored.trickRoomTurnsLeft != 3 || restored.pokeballCounts[0] != 5) return 44;
    for (uint8_t ball = 1; ball < 5; ++ball) if (restored.pokeballCounts[ball]) return 45;
    NativeRunSave invalidSetup = original;
    invalidSetup.pokeballCounts[1] = 1;
    if (validateNativeRunSave(invalidSetup, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 46;
    NativeStarterCandyRecord candyRecords[] = {{1, 7, 24}, {4, 8, 12}};
    char candyBytes[256]{};
    size_t candySize = 0;
    if (encodeNativeStarterCandyProfile(candyRecords, 2, 1, PokerogueContent::kContentHash,
            9999, candyBytes, sizeof(candyBytes), candySize) != NativeSaveResult::Ok) return 47;
    NativeStarterCandyRecord restoredCandy[2]{};
    size_t candyCount = 0;
    uint32_t candyGeneration = 0;
    if (decodeNativeStarterCandyProfile(candyBytes, candySize, PokerogueContent::kContentHash, 9999,
            restoredCandy, 2, candyCount, candyGeneration) != NativeSaveResult::Ok || candyCount != 2 ||
        candyGeneration != 1 || restoredCandy[0].candyCount != 7 || restoredCandy[1].friendship != 12)
        return 48;
    candyBytes[80] ^= 1;
    if (decodeNativeStarterCandyProfile(candyBytes, candySize, PokerogueContent::kContentHash, 9999,
            restoredCandy, 2, candyCount, candyGeneration) != NativeSaveResult::ChecksumMismatch ||
        restoredCandy[0].candyCount != 7 || candyCount != 2) return 49;
    candyRecords[1].speciesDex = 1;
    if (encodeNativeStarterCandyProfile(candyRecords, 2, 1, PokerogueContent::kContentHash, 9999,
            candyBytes, sizeof(candyBytes), candySize) != NativeSaveResult::InvalidRecord || candySize) return 50;
    static char profileScratch[2 * kStarterCandyProfileMaxBytes]{};
    other.sizes[0] = other.sizes[1] = 0;
    other.interrupt = false;
    NativeStarterCandyStore candyStore(other, profileScratch, sizeof(profileScratch));
    candyRecords[1].speciesDex = 4;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, 9999, candyGeneration) !=
            NativeSaveResult::Ok || candyGeneration != 1) return 51;
    candyRecords[0].candyCount = 10;
    other.interrupt = true;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, 9999, candyGeneration) !=
            NativeSaveResult::IoError || candyGeneration != 1) return 52;
    if (candyStore.load(PokerogueContent::kContentHash, 9999, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 7 || candyGeneration != 1) return 53;
    other.interrupt = false;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, 9999, candyGeneration) !=
            NativeSaveResult::Ok || candyGeneration != 2 ||
        candyStore.exportLatest(PokerogueContent::kContentHash, 9999) != NativeSaveResult::Ok ||
        decodeNativeStarterCandyProfile(other.exported, other.exportSize, PokerogueContent::kContentHash,
            9999, restoredCandy, 2, candyCount, candyGeneration) != NativeSaveResult::Ok ||
        restoredCandy[0].candyCount != 10) return 54;
    other.slots[1][80] ^= 1;
    if (candyStore.load(PokerogueContent::kContentHash, 9999, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 7 || candyGeneration != 1) return 55;
    if (encodeNativeStarterCandyProfile(candyRecords, 2, 1, PokerogueContent::kContentHash, 9999,
            other.slots[1], sizeof(other.slots[1]), other.sizes[1]) != NativeSaveResult::Ok ||
        candyStore.load(PokerogueContent::kContentHash, 9999, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::AmbiguousJournal || restoredCandy[0].candyCount != 7) return 56;
    const auto* candySpecies = PokerogueContent::findSpeciesByDex(1);
    if (!candySpecies || PokerogueContent::kMaxStarterCandyCount != 9999 ||
        PokerogueContent::kClassicCandyFriendshipMultiplier != 3) return 57;
    uint32_t rootCap = 0;
    for (const auto& entry : PokerogueContent::kStarterCandyFriendshipCaps)
        if (entry.cost == candySpecies->starterCost) rootCap = entry.value;
    if (!rootCap) return 58;
    NativeStarterCandyRecord awarding{1, 7, rootCap - 1};
    StarterCandyAwardEvent award{};
    if (applyNativeStarterCandyFriendship(awarding, rootCap + 2, award) != StarterCandyApplyResult::Applied ||
        awarding.candyCount != 9 || awarding.friendship != 1 || award.requestedAward != 2 ||
        award.appliedAward != 2) return 59;
    awarding.candyCount = PokerogueContent::kMaxStarterCandyCount;
    if (applyNativeStarterCandyFriendship(awarding, rootCap, award) != StarterCandyApplyResult::Applied ||
        awarding.friendship != rootCap - 1 || award.requestedAward || award.appliedAward) return 60;
    awarding.candyCount = PokerogueContent::kMaxStarterCandyCount - 1;
    if (applyNativeStarterCandyFriendship(awarding, rootCap * 3, award) != StarterCandyApplyResult::Applied ||
        awarding.candyCount != PokerogueContent::kMaxStarterCandyCount || award.requestedAward != 3 ||
        award.appliedAward != 1) return 61;
    awarding.friendship = 0xffffffffU;
    if (applyNativeStarterCandyFriendship(awarding, 1, award) != StarterCandyApplyResult::Overflow ||
        awarding.friendship != 0xffffffffU || award.appliedAward != 1) return 62;
    other.sizes[0] = other.sizes[1] = 0;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, candyGeneration) != NativeSaveResult::Ok ||
        candyStore.load(PokerogueContent::kContentHash, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 10 ||
        candyStore.exportLatest(PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 63;
    disk.sizes[0] = disk.sizes[1] = 0;
    disk.interrupt = false;
    disk.exportSize = other.exportSize;
    std::memcpy(disk.exported, other.exported, disk.exportSize);
    NativeStarterCandyStore profileImporter(disk, profileScratch, sizeof(profileScratch));
    size_t importedCandyCount = 0;
    uint32_t importedCandyGeneration = 0;
    NativeStarterCandyRecord importStaging[2]{};
    if (profileImporter.importExport(PokerogueContent::kContentHash, importStaging, 2,
            importedCandyCount, importedCandyGeneration) != NativeSaveResult::Ok || importedCandyCount != 2 ||
        importedCandyGeneration != 1 ||
        profileImporter.load(PokerogueContent::kContentHash, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 10) return 64;
    if (profileImporter.importExport(PokerogueContent::kContentHash, importStaging, 2,
            importedCandyCount, importedCandyGeneration) != NativeSaveResult::Ok || importedCandyGeneration != 2)
        return 65;
    disk.exported[80] ^= 1;
    if (profileImporter.importExport(PokerogueContent::kContentHash, importStaging, 2,
            importedCandyCount, importedCandyGeneration) != NativeSaveResult::ChecksumMismatch ||
        importedCandyGeneration != 2 || importedCandyCount != 2 ||
        profileImporter.load(PokerogueContent::kContentHash, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || candyGeneration != 2 || restoredCandy[0].candyCount != 10) return 66;
    other.sizes[0] = other.sizes[1] = 0;
    candyRecords[0].candyCount = 7;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, candyGeneration) != NativeSaveResult::Ok ||
        candyGeneration != 1) return 67;
    candyRecords[0].candyCount = 10;
    uint32_t preparedGeneration = 0;
    if (candyStore.prepareFromCommitted(candyRecords, 2, PokerogueContent::kContentHash, 1,
            preparedGeneration) != NativeSaveResult::Ok || preparedGeneration != 2) return 68;
    // Simulate power loss before the run stores its new profile generation.
    if (candyStore.loadGeneration(PokerogueContent::kContentHash, 1, restoredCandy, 2,
            candyCount, candyGeneration) != NativeSaveResult::Ok || restoredCandy[0].candyCount != 7)
        return 69;
    if (candyStore.prepareFromCommitted(candyRecords, 2, PokerogueContent::kContentHash, 1,
            preparedGeneration) != NativeSaveResult::Ok || preparedGeneration != 3 ||
        candyStore.loadGeneration(PokerogueContent::kContentHash, 1, restoredCandy, 2,
            candyCount, candyGeneration) != NativeSaveResult::Ok || restoredCandy[0].candyCount != 7)
        return 70;
    if (candyStore.loadGeneration(PokerogueContent::kContentHash, 3, restoredCandy, 2,
            candyCount, candyGeneration) != NativeSaveResult::Ok || restoredCandy[0].candyCount != 10 ||
        candyGeneration != 3) return 71;
    if (candyStore.loadGeneration(PokerogueContent::kContentHash, 2, restoredCandy, 2,
            candyCount, candyGeneration) != NativeSaveResult::NotFound || candyGeneration != 3 ||
        restoredCandy[0].candyCount != 10) return 72;
    original.starterProfileGeneration = 3;
    size_t linkedSize = 0;
    if (encodeNativeRunSave(original, partyBytes, sizeof(partyBytes), linkedSize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, linkedSize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.starterProfileGeneration != 3) return 73;
    size_t oldPayloadSize = 0;
    for (size_t n = 0; n < linkedSize; ++n) {
        if (n + 25 <= linkedSize && std::memcmp(partyBytes + n, "starterProfileGeneration=", 25) == 0) {
            oldPayloadSize = n; break;
        }
    }
    if (!oldPayloadSize) return 74;
    for (size_t n = 0; n < oldPayloadSize; ++n) {
        if (n + 16 <= oldPayloadSize && std::memcmp(partyBytes + n, "saveVersion=", 12) == 0)
            std::memcpy(partyBytes + n + 12, "000b", 4);
        if (n + 19 <= oldPayloadSize && std::memcmp(partyBytes + n, "runtimeVersion=", 15) == 0)
            std::memcpy(partyBytes + n + 15, "000b", 4);
    }
    IntegritySha256::hashHex(partyBytes, oldPayloadSize, digest);
    std::memcpy(partyBytes + oldPayloadSize, "sha256=", 7);
    std::memcpy(partyBytes + oldPayloadSize + 7, digest, 64);
    partyBytes[oldPayloadSize + 71] = '\n';
    if (decodeNativeRunSave(partyBytes, oldPayloadSize + 72, PokerogueContent::kContentHash, restored) !=
            NativeSaveResult::Ok || restored.starterProfileGeneration != 0 ||
        restored.saveVersion != kNativeSaveVersion || restored.runtimeVersion != kNativeSaveRuntimeVersion)
        return 75;
    disk.sizes[0] = disk.sizes[1] = 0;
    other.sizes[0] = other.sizes[1] = 0;
    NativeProgressStore progress(store, candyStore);
    NativeRunSave committedRun{};
    if (makeNativeRunSetupSave(123, 1, committedRun) != NativeSaveResult::Ok) return 76;
    candyRecords[0].candyCount = 7;
    if (progress.commit(committedRun, candyRecords, 2) != NativeSaveResult::Ok ||
        committedRun.starterProfileGeneration != 1 || committedRun.generation != 1) return 77;
    candyRecords[0].candyCount = 10;
    disk.interrupt = true;
    if (progress.commit(committedRun, candyRecords, 2) != NativeSaveResult::IoError ||
        committedRun.starterProfileGeneration != 1) return 78;
    disk.interrupt = false;
    if (progress.load(PokerogueContent::kContentHash, restored, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || restored.starterProfileGeneration != 1 ||
        restoredCandy[0].candyCount != 7) return 79;
    if (progress.commit(committedRun, candyRecords, 2) != NativeSaveResult::Ok ||
        committedRun.starterProfileGeneration != 3 ||
        progress.load(PokerogueContent::kContentHash, restored, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 10) return 80;
    NativeRunSave staleRun = committedRun;
    staleRun.starterProfileGeneration = 1;
    if (progress.commit(staleRun, candyRecords, 2) != NativeSaveResult::InvalidRecord) return 81;
    other.interrupt = true;
    candyRecords[0].candyCount = 11;
    if (progress.commit(committedRun, candyRecords, 2) != NativeSaveResult::IoError) return 82;
    other.interrupt = false;
    if (progress.load(PokerogueContent::kContentHash, restored, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || restored.starterProfileGeneration != 3 ||
        restoredCandy[0].candyCount != 10) return 83;
    candyRecords[0].candyCount = 12;
    if (candyStore.prepareFromCommitted(candyRecords, 2, PokerogueContent::kContentHash, 3,
            preparedGeneration) != NativeSaveResult::Ok ||
        candyStore.exportGeneration(PokerogueContent::kContentHash, 3) != NativeSaveResult::Ok) return 84;
    if (decodeNativeStarterCandyProfile(other.exported, other.exportSize, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || candyGeneration != 3 || restoredCandy[0].candyCount != 10) return 85;
    if (candyStore.exportGeneration(PokerogueContent::kContentHash, 2) != NativeSaveResult::NotFound)
        return 86;
    if (candyStore.inspectGeneration(PokerogueContent::kContentHash, 3) != NativeSaveResult::Ok ||
        candyStore.inspectGeneration(PokerogueContent::kContentHash, 2) != NativeSaveResult::NotFound)
        return 87;
    const uint32_t unchangedSeed = restored.seed;
    other.sizes[0] = other.sizes[1] = 0;
    if (store.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::InvalidRecord ||
        restored.seed != unchangedSeed ||
        store.save(committedRun) != NativeSaveResult::InvalidRecord) return 88;
    NativeRunSaveStore unboundStore(disk);
    if (unboundStore.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::InvalidRecord)
        return 89;
    trainerSave.participantHistoryResolved = true;
    trainerSave.participantCount = 2;
    trainerSave.participantIds[0] = 0;
    trainerSave.participantIds[1] = 123;
    size_t historySize = 0;
    if (encodeNativeRunSave(trainerSave, partyBytes, sizeof(partyBytes), historySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, historySize, PokerogueContent::kContentHash, restored) !=
            NativeSaveResult::Ok || !restored.participantHistoryResolved || restored.participantCount != 2 ||
        restored.participantIds[0] != 0 || restored.participantIds[1] != 123) return 90;
    trainerSave.participantIds[1] = 0;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 91;
    trainerSave.participantIds[1] = 123;
    trainerSave.participantHistoryResolved = false;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 92;
    size_t profilePayloadSize = 0;
    for (size_t n = 0; n < historySize; ++n) {
        if (n + 27 <= historySize && std::memcmp(partyBytes + n, "participantHistoryResolved=", 27) == 0) {
            profilePayloadSize = n; break;
        }
        if (n + 16 <= historySize && std::memcmp(partyBytes + n, "saveVersion=", 12) == 0)
            std::memcpy(partyBytes + n + 12, "000c", 4);
        if (n + 19 <= historySize && std::memcmp(partyBytes + n, "runtimeVersion=", 15) == 0)
            std::memcpy(partyBytes + n + 15, "000c", 4);
    }
    if (!profilePayloadSize) return 93;
    IntegritySha256::hashHex(partyBytes, profilePayloadSize, digest);
    std::memcpy(partyBytes + profilePayloadSize, "sha256=", 7);
    std::memcpy(partyBytes + profilePayloadSize + 7, digest, 64);
    partyBytes[profilePayloadSize + 71] = '\n';
    if (decodeNativeRunSave(partyBytes, profilePayloadSize + 72, PokerogueContent::kContentHash, restored) !=
            NativeSaveResult::Ok || restored.participantHistoryResolved || restored.participantCount ||
        restored.saveVersion != kNativeSaveVersion) return 94;
    static char bundleProfile[kStarterCandyProfileMaxBytes]{};
    static char bundleBytes[kNativeProgressBundleMaxBytes]{};
    static char repeatedBundle[kNativeProgressBundleMaxBytes]{};
    original.starterProfileGeneration = 17;
    size_t bundleRunSize = 0, bundleProfileSize = 0, bundleSize = 0, repeatedSize = 0;
    if (encodeNativeRunSave(original, partyBytes, sizeof(partyBytes), bundleRunSize) != NativeSaveResult::Ok ||
        encodeNativeStarterCandyProfile(candyRecords, 2, 17, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, bundleProfile, sizeof(bundleProfile), bundleProfileSize) !=
            NativeSaveResult::Ok || encodeNativeProgressBundle(partyBytes, bundleRunSize, bundleProfile,
            bundleProfileSize, PokerogueContent::kContentHash, restored, bundleBytes, sizeof(bundleBytes), bundleSize) !=
            NativeSaveResult::Ok) return 95;
    NativeProgressBundleView bundleView{};
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, PokerogueContent::kContentHash,
            restored, bundleView) != NativeSaveResult::Ok || bundleView.sourceProfileGeneration != 17 ||
        bundleView.runSize != bundleRunSize || bundleView.profileSize != bundleProfileSize) return 96;
    if (encodeNativeProgressBundle(partyBytes, bundleRunSize, bundleProfile, bundleProfileSize,
            PokerogueContent::kContentHash, restored, repeatedBundle, sizeof(repeatedBundle), repeatedSize) !=
            NativeSaveResult::Ok || bundleSize != repeatedSize || std::memcmp(bundleBytes, repeatedBundle, bundleSize))
        return 97;
    bundleBytes[16 + bundleRunSize + 80] ^= 1;
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, PokerogueContent::kContentHash, restored,
            bundleView) != NativeSaveResult::ChecksumMismatch || bundleView.sourceProfileGeneration != 17)
        return 98;
    bundleBytes[16 + bundleRunSize + 80] ^= 1;
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, wrongHash, restored, bundleView) !=
            NativeSaveResult::ContentMismatch) return 99;
    if (encodeNativeStarterCandyProfile(candyRecords, 2, 18, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, bundleProfile, sizeof(bundleProfile), bundleProfileSize) !=
            NativeSaveResult::Ok || encodeNativeProgressBundle(partyBytes, bundleRunSize, bundleProfile,
            bundleProfileSize, PokerogueContent::kContentHash, restored, repeatedBundle, sizeof(repeatedBundle), repeatedSize) !=
            NativeSaveResult::InvalidRecord || repeatedSize) return 100;
    bundleBytes[7] = '2';
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, PokerogueContent::kContentHash, restored,
            bundleView) != NativeSaveResult::UnsupportedVersion) return 101;
    bundleBytes[7] = '1';
    StarterCandyProfileCodec::put(0xffffffffU, bundleBytes + 8, 4);
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, PokerogueContent::kContentHash, restored,
            bundleView) != NativeSaveResult::InvalidFormat) return 102;
    // Real paired journal -> SD-like bundle transport -> foreign generation rebase.
    disk.sizes[0] = disk.sizes[1] = other.sizes[0] = other.sizes[1] = 0;
    NativeRunSave portable{};
    if (makeNativeRunSetupSave(321, 1, portable) != NativeSaveResult::Ok ||
        progress.commit(portable, candyRecords, 2) != NativeSaveResult::Ok) return 103;
    static MemoryBundleStorage transport;
    static char bundleWorkspace[2 * kNativeProgressBundleMaxBytes]{};
    if (progress.exportBundle(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restoredCandy, 2) != NativeSaveResult::Ok) return 104;
    size_t importedCount = 0;
    if (progress.readBundleCandidate(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restored, restoredCandy, 2, importedCount) != NativeSaveResult::Ok ||
        restored.seed != 321 || importedCount != 2 || restored.starterProfileGeneration != 1) return 105;
    // Advance local generation independently; the portable reference stays at one.
    portable.seed = 654;
    if (progress.commit(portable, candyRecords, 2) != NativeSaveResult::Ok ||
        portable.starterProfileGeneration != 2 ||
        progress.commitImported(restored, restoredCandy, importedCount) != NativeSaveResult::Ok ||
        restored.seed != 321 || restored.starterProfileGeneration != 3) return 106;
    transport.bytes[16] ^= 1;
    importedCount = 999;
    if (progress.readBundleCandidate(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restored, restoredCandy, 2, importedCount) != NativeSaveResult::ChecksumMismatch ||
        importedCount != 999 || progress.load(PokerogueContent::kContentHash, portable,
            restoredCandy, 2, candyCount) != NativeSaveResult::Ok || portable.starterProfileGeneration != 3)
        return 107;
    transport.bytes[16] ^= 1;
    transport.interrupt = true;
    if (progress.exportBundle(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restoredCandy, 2) != NativeSaveResult::IoError ||
        progress.load(PokerogueContent::kContentHash, portable, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || portable.starterProfileGeneration != 3) return 108;
    transport.interrupt = false;
    if (progress.exportBundle(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restoredCandy, 2) != NativeSaveResult::Ok ||
        progress.readBundleCandidate(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restored, restoredCandy, 2, importedCount) != NativeSaveResult::Ok)
        return 109;
    disk.interrupt = true;
    if (progress.commitImported(restored, restoredCandy, importedCount) != NativeSaveResult::IoError)
        return 110;
    disk.interrupt = false;
    if (progress.load(PokerogueContent::kContentHash, portable, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || portable.starterProfileGeneration != 3 || portable.seed != 321)
        return 111;
    NativeStarterCandyRecord caughtProfile[1]{{1, 0, 0, true}};
    char caughtEncoded[256]{};
    size_t caughtWritten = 0;
    if (encodeNativeStarterCandyProfile(caughtProfile, 1, 1, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtEncoded, sizeof(caughtEncoded), caughtWritten) !=
            NativeSaveResult::Ok) return 112;
    NativeStarterCandyRecord caughtDecoded[1]{};
    size_t caughtCount = 0;
    uint32_t caughtGeneration = 0;
    if (decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || caughtCount != 1 || !caughtDecoded[0].caught) return 113;
    NativeStarterCandyRecord purchased{1, 999, 321, true};
    const PokerogueContent::StarterCandyPrice* starterPrice = nullptr;
    const auto* purchasedSpecies = PokerogueContent::findSpeciesByDex(1);
    for (const auto& row : PokerogueContent::kStarterCandyPrices)
        if (row.cost == purchasedSpecies->starterCost) { starterPrice = &row; break; }
    if (!starterPrice || applyNativeStarterCostReduction(purchased) != StarterCostPurchaseResult::Applied ||
        purchased.costReduction != 1 || purchased.candyCount != 999 - starterPrice->costReduction[0] ||
        purchased.friendship != 321 || !purchased.caught) return 117;
    if (applyNativeStarterCostReduction(purchased) != StarterCostPurchaseResult::Applied ||
        purchased.costReduction != 2 || purchased.candyCount !=
            999 - starterPrice->costReduction[0] - starterPrice->costReduction[1]) return 118;
    purchased.observedFormAttr = 128;
    purchased.unlockedFormAttr = 128;
    purchased.abilityAttr = 5;
    purchased.genderAttr = 12;
    purchased.natureAttr = (1u << 1) | (1u << 25);
    const uint8_t savedIvs[] = {31, 0, 7, 15, 22, 30};
    for (uint8_t i = 0; i < 6; ++i) purchased.dexIvs[i] = savedIvs[i];
    const uint16_t remainingCandy = purchased.candyCount;
    if (applyNativeStarterCostReduction(purchased) != StarterCostPurchaseResult::MaximumReduction ||
        purchased.candyCount != remainingCandy || purchased.costReduction != 2) return 119;
    NativeStarterCandyRecord poor{1, 0, 42, true};
    if (applyNativeStarterCostReduction(poor) != StarterCostPurchaseResult::InsufficientCandy ||
        poor.candyCount || poor.costReduction || poor.friendship != 42 || !poor.caught) return 120;
    if (encodeNativeStarterCandyProfile(&purchased, 1, 1, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtEncoded, sizeof(caughtEncoded), caughtWritten) !=
            NativeSaveResult::Ok || std::memcmp(caughtEncoded, "P3CANDYB", 8) ||
        decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || caughtDecoded[0].costReduction != 2 || !caughtDecoded[0].caught ||
        caughtDecoded[0].candyCount != remainingCandy || caughtDecoded[0].friendship != 321) return 121;
    if (caughtDecoded[0].natureAttr != purchased.natureAttr || caughtDecoded[0].abilityAttr != 5 ||
        caughtDecoded[0].genderAttr != 12 || caughtDecoded[0].observedFormAttr != 128) return 125;
    for (uint8_t i = 0; i < 6; ++i) if (caughtDecoded[0].dexIvs[i] != savedIvs[i]) return 126;
    PokemonActorIdentity appearanceActor{};uint8_t appearanceBits=255;
    if(!nativePokemonAppearanceAttr(appearanceActor,appearanceBits) || appearanceBits) return 706;
    appearanceActor.shiny = true;
    if (nativePokemonAppearanceAttr(appearanceActor, appearanceBits)) return 717;
    appearanceActor.shiny = false; appearanceActor.shinyVariant = 1;
    if (nativePokemonAppearanceAttr(appearanceActor, appearanceBits)) return 718;
    appearanceActor.shinyVariant = 0;
    appearanceActor.appearanceResolved=true;
    if(!nativePokemonAppearanceAttr(appearanceActor,appearanceBits) || appearanceBits!=17) return 707;
    appearanceActor.shiny=true;
    for(uint8_t variant=0;variant<3;++variant) {
        appearanceActor.shinyVariant=variant;
        if(!nativePokemonAppearanceAttr(appearanceActor,appearanceBits) || appearanceBits!=(2u | (16u<<variant))) return 708;
    }
    appearanceActor.shiny=false;
    if(nativePokemonAppearanceAttr(appearanceActor,appearanceBits)) return 709;
    purchased.observedAppearanceAttr=0x73;
    purchased.caughtAppearanceAttr=0x32; // Shiny rare variant, source DexAttr bits.
    if(encodeNativeStarterCandyProfile(&purchased,1,1,PokerogueContent::kContentHash,
        PokerogueContent::kMaxStarterCandyCount,caughtEncoded,sizeof(caughtEncoded),caughtWritten)!=NativeSaveResult::Ok ||
        decodeNativeStarterCandyProfile(caughtEncoded,caughtWritten,PokerogueContent::kContentHash,
        PokerogueContent::kMaxStarterCandyCount,caughtDecoded,1,caughtCount,caughtGeneration)!=NativeSaveResult::Ok ||
        caughtDecoded[0].observedAppearanceAttr!=0x73 || caughtDecoded[0].caughtAppearanceAttr!=0x32) return 701;
    // New preference persists; v9 reads without fabricating a selected slot.
    purchased.preferredAbilityIndex=2;
    if(encodeNativeStarterCandyProfile(&purchased,1,1,PokerogueContent::kContentHash,
        PokerogueContent::kMaxStarterCandyCount,caughtEncoded,sizeof(caughtEncoded),caughtWritten)!=NativeSaveResult::Ok ||
        decodeNativeStarterCandyProfile(caughtEncoded,caughtWritten,PokerogueContent::kContentHash,
        PokerogueContent::kMaxStarterCandyCount,caughtDecoded,1,caughtCount,caughtGeneration)!=NativeSaveResult::Ok ||
        caughtDecoded[0].preferredAbilityIndex!=2) return 719;
    char v9Bytes[256]{};
    std::memcpy(v9Bytes,caughtEncoded,121);std::memcpy(v9Bytes,"P3CANDY9",8);
    const size_t v9Size=kStarterCandyProfileOverhead+41;
    IntegritySha256::hashHex(v9Bytes,v9Size-64,digest);std::memcpy(v9Bytes+v9Size-64,digest,64);
    if(decodeNativeStarterCandyProfile(v9Bytes,v9Size,PokerogueContent::kContentHash,
        PokerogueContent::kMaxStarterCandyCount,caughtDecoded,1,caughtCount,caughtGeneration)!=NativeSaveResult::Ok ||
        caughtDecoded[0].preferredAbilityIndex!=255 || caughtDecoded[0].caughtAppearanceAttr!=0x32) return 720;
    auto invalidPreference=purchased;invalidPreference.preferredAbilityIndex=3;
    if(StarterCandyProfileCodec::valid(invalidPreference,0,PokerogueContent::kMaxStarterCandyCount)) return 721;
    invalidPreference.preferredAbilityIndex=1; // Bulbasaur's duplicate slot is not unlocked.
    if(StarterCandyProfileCodec::valid(invalidPreference,0,PokerogueContent::kMaxStarterCandyCount)) return 722;
    invalidPreference=purchased;invalidPreference.caught=false;invalidPreference.caughtAppearanceAttr=0;
    invalidPreference.passiveUnlocked=false;invalidPreference.preferredFormIndex=65535;
    if(StarterCandyProfileCodec::valid(invalidPreference,0,PokerogueContent::kMaxStarterCandyCount)) return 723;
    // v11 persists all unlocked nature enum values independently of ability.
    for(unsigned nature=0;nature<25;++nature) {
        auto naturePreference=purchased;naturePreference.natureAttr=1u<<(nature+1);
        naturePreference.preferredNatureIndex=nature;
        if(encodeNativeStarterCandyProfile(&naturePreference,1,1,PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount,caughtEncoded,sizeof(caughtEncoded),caughtWritten)!=NativeSaveResult::Ok ||
            decodeNativeStarterCandyProfile(caughtEncoded,caughtWritten,PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount,caughtDecoded,1,caughtCount,caughtGeneration)!=NativeSaveResult::Ok ||
            caughtDecoded[0].preferredNatureIndex!=nature || caughtDecoded[0].preferredAbilityIndex!=2) return 746;
    }
    auto naturePreference=purchased;naturePreference.preferredNatureIndex=25;
    if(StarterCandyProfileCodec::valid(naturePreference,0,PokerogueContent::kMaxStarterCandyCount)) return 747;
    naturePreference.preferredNatureIndex=1; // Only Hardy/Quirky unlocked in purchased.
    if(StarterCandyProfileCodec::valid(naturePreference,0,PokerogueContent::kMaxStarterCandyCount)) return 748;
    char v10Bytes[256]{};
    std::memcpy(v10Bytes,caughtEncoded,122);std::memcpy(v10Bytes,"P3CANDYA",8);
    const size_t v10Size=kStarterCandyProfileOverhead+42;
    IntegritySha256::hashHex(v10Bytes,v10Size-64,digest);std::memcpy(v10Bytes+v10Size-64,digest,64);
    if(decodeNativeStarterCandyProfile(v10Bytes,v10Size,PokerogueContent::kContentHash,
        PokerogueContent::kMaxStarterCandyCount,caughtDecoded,1,caughtCount,caughtGeneration)!=NativeSaveResult::Ok ||
        caughtDecoded[0].preferredNatureIndex!=255 || caughtDecoded[0].preferredAbilityIndex!=2) return 749;
    // Unknown future version cannot mutate decoded records/count/generation.
    std::memcpy(v10Bytes,"P3CANDYC",8);
    IntegritySha256::hashHex(v10Bytes,v10Size-64,digest);std::memcpy(v10Bytes+v10Size-64,digest,64);
    caughtCount=77;caughtGeneration=88;caughtDecoded[0].preferredNatureIndex=9;
    if(decodeNativeStarterCandyProfile(v10Bytes,v10Size,PokerogueContent::kContentHash,
        PokerogueContent::kMaxStarterCandyCount,caughtDecoded,1,caughtCount,caughtGeneration)!=NativeSaveResult::UnsupportedVersion ||
        caughtCount!=77 || caughtGeneration!=88 || caughtDecoded[0].preferredNatureIndex!=9) return 750;
    purchased.preferredAbilityIndex=255;
    // Restore the current encoded baseline for the older-version cases below.
    if(encodeNativeStarterCandyProfile(&purchased,1,1,PokerogueContent::kContentHash,
        PokerogueContent::kMaxStarterCandyCount,caughtEncoded,sizeof(caughtEncoded),caughtWritten)!=NativeSaveResult::Ok) return 751;
    char v8Bytes[256]{};
    std::memcpy(v8Bytes,caughtEncoded,119);std::memcpy(v8Bytes,"P3CANDY8",8);
    const size_t v8Size=kStarterCandyProfileOverhead+39;
    IntegritySha256::hashHex(v8Bytes,v8Size-64,digest);std::memcpy(v8Bytes+v8Size-64,digest,64);
    if(decodeNativeStarterCandyProfile(v8Bytes,v8Size,PokerogueContent::kContentHash,
        PokerogueContent::kMaxStarterCandyCount,caughtDecoded,1,caughtCount,caughtGeneration)!=NativeSaveResult::Ok ||
        caughtDecoded[0].observedAppearanceAttr || caughtDecoded[0].caughtAppearanceAttr ||
        !caughtDecoded[0].caught) return 702; // Old caught metadata does not invent appearance.
    bool defaultShiny = false;
    uint8_t defaultVariant = 0;
    auto appearancePreference = purchased;
    appearancePreference.caughtAppearanceAttr = 0x73;
    if (!nativeStarterDefaultAppearance(appearancePreference, defaultShiny, defaultVariant) ||
        !defaultShiny || defaultVariant != 2) return 710;
    if (!nativeStarterDefaultAppearance(appearancePreference, defaultShiny, defaultVariant, false) ||
        defaultShiny || defaultVariant) return 711;
    appearancePreference.caughtAppearanceAttr = 0x22;
    if (!nativeStarterDefaultAppearance(appearancePreference, defaultShiny, defaultVariant, false) ||
        !defaultShiny || defaultVariant != 1) return 712;
    appearancePreference.caughtAppearanceAttr = 17;
    if (!nativeStarterDefaultAppearance(appearancePreference, defaultShiny, defaultVariant) ||
        defaultShiny || defaultVariant) return 713;
    defaultShiny = true; defaultVariant = 2;
    appearancePreference.caughtAppearanceAttr = 0;
    if (nativeStarterDefaultAppearance(appearancePreference, defaultShiny, defaultVariant) ||
        !defaultShiny || defaultVariant != 2) return 714;
    appearancePreference.caughtAppearanceAttr = 0x21;
    if (nativeStarterDefaultAppearance(appearancePreference, defaultShiny, defaultVariant)) return 715;
    appearancePreference.caughtAppearanceAttr = 17; appearancePreference.caught = false;
    if (nativeStarterDefaultAppearance(appearancePreference, defaultShiny, defaultVariant)) return 716;
    auto invalidAppearance=purchased;invalidAppearance.caughtAppearanceAttr=0x30;
    if(StarterCandyProfileCodec::valid(invalidAppearance,0,PokerogueContent::kMaxStarterCandyCount)) return 703;
    invalidAppearance=purchased;invalidAppearance.observedAppearanceAttr=0x21;
    if(StarterCandyProfileCodec::valid(invalidAppearance,0,PokerogueContent::kMaxStarterCandyCount)) return 704;
    invalidAppearance=purchased;invalidAppearance.caught=false;
    if(StarterCandyProfileCodec::valid(invalidAppearance,0,PokerogueContent::kMaxStarterCandyCount)) return 705;
    char v3Bytes[256]{};
    std::memcpy(v3Bytes, caughtEncoded, 89);
    std::memcpy(v3Bytes, "P3CANDY3", 8);
    const size_t v3Size = kStarterCandyProfileOverhead + 9;
    char v3Digest[65]{};
    IntegritySha256::hashHex(v3Bytes, v3Size - 64, v3Digest);
    std::memcpy(v3Bytes + v3Size - 64, v3Digest, 64);
    NativeStarterCandyRecord v3Decoded[1]{};
    if (decodeNativeStarterCandyProfile(v3Bytes, v3Size, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, v3Decoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || v3Decoded[0].costReduction != 2 || !v3Decoded[0].caught ||
        v3Decoded[0].natureAttr) return 130;
    char v4Bytes[256]{};
    std::memcpy(v4Bytes, caughtEncoded, 99);
    std::memcpy(v4Bytes, "P3CANDY4", 8);
    const size_t v4Size = kStarterCandyProfileOverhead + 19;
    char v4Digest[65]{};
    IntegritySha256::hashHex(v4Bytes, v4Size - 64, v4Digest);
    std::memcpy(v4Bytes + v4Size - 64, v4Digest, 64);
    NativeStarterCandyRecord v4Decoded[1]{};
    if (decodeNativeStarterCandyProfile(v4Bytes, v4Size, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, v4Decoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || v4Decoded[0].natureAttr != purchased.natureAttr ||
        v4Decoded[0].abilityAttr || v4Decoded[0].genderAttr || v4Decoded[0].dexIvs[0] != 31) return 131;
    auto invalidAttributes = purchased;
    invalidAttributes.abilityAttr = 8;
    if (StarterCandyProfileCodec::valid(invalidAttributes, 0, PokerogueContent::kMaxStarterCandyCount)) return 132;
    invalidAttributes = purchased;
    invalidAttributes.genderAttr = 1;
    if (StarterCandyProfileCodec::valid(invalidAttributes, 0, PokerogueContent::kMaxStarterCandyCount)) return 133;
    char v5Bytes[256]{};
    std::memcpy(v5Bytes, caughtEncoded, 101);
    std::memcpy(v5Bytes, "P3CANDY5", 8);
    const size_t v5Size = kStarterCandyProfileOverhead + 21;
    char v5Digest[65]{};
    IntegritySha256::hashHex(v5Bytes, v5Size - 64, v5Digest);
    std::memcpy(v5Bytes + v5Size - 64, v5Digest, 64);
    NativeStarterCandyRecord v5Decoded[1]{};
    if (decodeNativeStarterCandyProfile(v5Bytes, v5Size, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, v5Decoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || v5Decoded[0].abilityAttr != 5 || v5Decoded[0].genderAttr != 12 ||
        v5Decoded[0].natureAttr != purchased.natureAttr || v5Decoded[0].observedFormAttr) return 149;
    auto invalidObserved = purchased;
    char v6Bytes[256]{};
    const size_t v6Size = kStarterCandyProfileOverhead + 29;
    std::memcpy(v6Bytes, caughtEncoded, 109);
    std::memcpy(v6Bytes, "P3CANDY6", 8);
    char v6Digest[65]{};
    IntegritySha256::hashHex(v6Bytes, v6Size - 64, v6Digest);
    std::memcpy(v6Bytes + v6Size - 64, v6Digest, 64);
    NativeStarterCandyRecord v6Decoded[1]{};
    if (decodeNativeStarterCandyProfile(v6Bytes, v6Size, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, v6Decoded, 1, caughtCount, caughtGeneration) != NativeSaveResult::Ok ||
        v6Decoded[0].observedFormAttr != purchased.observedFormAttr || v6Decoded[0].unlockedFormAttr) return 161;
    if (caughtDecoded[0].unlockedFormAttr != purchased.unlockedFormAttr) return 162;
    auto invalidUnlock = purchased;
    invalidUnlock.unlockedFormAttr = 1;
    if (StarterCandyProfileCodec::valid(invalidUnlock, 0, PokerogueContent::kMaxStarterCandyCount)) return 163;
    invalidObserved.observedFormAttr = 1;
    if (StarterCandyProfileCodec::valid(invalidObserved, 0, PokerogueContent::kMaxStarterCandyCount)) return 150;
    invalidObserved.observedFormAttr = uint64_t(1) << 63;
    if (StarterCandyProfileCodec::valid(invalidObserved, 0, PokerogueContent::kMaxStarterCandyCount)) return 151;
    auto invalidDexRecord = purchased;
    invalidDexRecord.dexIvs[0] = 32;
    if (StarterCandyProfileCodec::valid(invalidDexRecord, 0, PokerogueContent::kMaxStarterCandyCount)) return 127;
    invalidDexRecord = purchased;
    invalidDexRecord.natureAttr |= 1u;
    if (StarterCandyProfileCodec::valid(invalidDexRecord, 0, PokerogueContent::kMaxStarterCandyCount)) return 128;
    PokemonNature defaultNature = PokemonNature::Unspecified;
    if (!nativeStarterDefaultNature(purchased, defaultNature) || defaultNature != PokemonNature::Hardy) return 134;
    // Every source enum index resolves only its own unlocked bit.
    for(unsigned firstNature=0;firstNature<25;++firstNature) {
        auto natureRecord=purchased;natureRecord.natureAttr=1u<<(firstNature+1);
        for(unsigned requested=0;requested<=25;++requested) {
            PokemonNature selected=PokemonNature::Unspecified;
            if(nativeStarterSelectedNature(natureRecord,requested,selected)!=(requested==firstNature)) return 738;
            if(requested!=firstNature && selected!=PokemonNature::Unspecified) return 739;
        }
        PokemonNature alternate=PokemonNature::Unspecified;
        if(nativeStarterNextNature(natureRecord,static_cast<PokemonNature>(firstNature),1,alternate) ||
            alternate!=PokemonNature::Unspecified) return 740;
        for(unsigned secondNature=0;secondNature<25;++secondNature) {
            if(secondNature==firstNature) continue;
            natureRecord.natureAttr=(1u<<(firstNature+1)) | (1u<<(secondNature+1));
            for(int direction:{-1,1}) {
                alternate=PokemonNature::Unspecified;
                if(!nativeStarterNextNature(natureRecord,static_cast<PokemonNature>(firstNature),direction,alternate) ||
                    static_cast<unsigned>(alternate)!=secondNature) return 741;
            }
        }
    }
    auto allNatures=purchased;allNatures.natureAttr=0x03fffffeu;
    for(unsigned currentNature=0;currentNature<25;++currentNature) {
        PokemonNature alternate=PokemonNature::Unspecified;
        if(!nativeStarterNextNature(allNatures,static_cast<PokemonNature>(currentNature),1,alternate) ||
            static_cast<unsigned>(alternate)!=(currentNature+1)%25) return 742;
        if(!nativeStarterNextNature(allNatures,static_cast<PokemonNature>(currentNature),-1,alternate) ||
            static_cast<unsigned>(alternate)!=(currentNature+24)%25) return 743;
    }
    PokemonNature rejectedNature=PokemonNature::Unspecified;
    allNatures.natureAttr|=1u;
    if(nativeStarterSelectedNature(allNatures,0,rejectedNature) || rejectedNature!=PokemonNature::Unspecified) return 744;
    if(nativeStarterNextNature(purchased,PokemonNature::Unspecified,1,rejectedNature) ||
        nativeStarterNextNature(purchased,PokemonNature::Hardy,0,rejectedNature) || rejectedNature!=PokemonNature::Unspecified) return 745;
    auto missingNature = purchased;
    missingNature.natureAttr = 0;
    if (nativeStarterDefaultNature(missingNature, defaultNature) || defaultNature != PokemonNature::Hardy) return 135;
    uint8_t defaultAbilityIndex = 255;
    uint16_t defaultAbility = 65535;
    if (!nativeStarterDefaultAbility(purchased, defaultAbilityIndex, defaultAbility) ||
        defaultAbilityIndex != 0 || defaultAbility != purchasedSpecies->ability1) return 136;
    // Canonical raw NONE is normalized by the runtime constructor, not by changing imports.
    for(const auto& species:PokerogueContent::kSpecies) {
        if(!species.starterEligible) continue;
        NativeStarterCandyRecord slotRecord{};slotRecord.speciesDex=species.dex;
        uint8_t slot=255;uint16_t ability=65535;
        for(unsigned mask=0;mask<16;++mask) {
            slotRecord.abilityAttr=mask;
            const bool duplicate=!species.ability2 || species.ability1==species.ability2;
            const unsigned expected=mask>7 ? 0 : (mask & 1u) |
                ((mask & 2u) && !(duplicate && (mask & 1u)) ? 2u : 0u) |
                ((mask & 4u) && species.abilityHidden ? 4u : 0u);
            if(nativeStarterAbilityChoiceMask(slotRecord)!=expected) return 159;
            for(unsigned candidate=0;candidate<4;++candidate) {
                uint16_t chosen=65535;
                const bool allowed=candidate<3 && (expected & (1u<<candidate));
                if(nativeStarterSelectedAbility(slotRecord,candidate,chosen)!=allowed) return 160;
                if(!allowed && chosen!=65535) return 161;
                if(allowed && chosen!=(candidate==0 ? species.ability1 : candidate==1 ?
                    (species.ability2 ? species.ability2 : species.ability1) : species.abilityHidden)) return 162;
            }
        }
        slotRecord.abilityAttr=2;
        if(!nativeStarterDefaultAbility(slotRecord,slot,ability) || slot!=1 ||
           ability!=(species.ability2 ? species.ability2 : species.ability1)) return 155;
        if(species.abilityHidden) {
            slotRecord.abilityAttr=4;
            if(!nativeStarterDefaultAbility(slotRecord,slot,ability) || slot!=2 || ability!=species.abilityHidden) return 156;
        }
        for(const auto& form:PokerogueContent::kForms) {
            if(std::strcmp(form.speciesId,species.id)) continue;
            const auto primary=form.ability1 ? form.ability1 : species.ability1;
            if(nativeStarterFormAbility(species,&form,1,species.ability1)!=
               (form.ability2 ? form.ability2 : primary)) return 157;
        }
        if(nativeStarterFormAbility(species,nullptr,3,species.ability1)) return 158;
        for(const auto& form:PokerogueContent::kForms) {
            if(!std::strcmp(form.speciesId,species.id)) continue;
            if(nativeStarterFormAbility(species,&form,0,species.ability1)) return 163;
            break;
        }
    }
    auto hiddenOnly = purchased;
    hiddenOnly.abilityAttr = 4;
    if (!nativeStarterDefaultAbility(hiddenOnly, defaultAbilityIndex, defaultAbility) ||
        defaultAbilityIndex != 2 || defaultAbility != purchasedSpecies->abilityHidden) return 137;
    hiddenOnly.abilityAttr = 0;
    const uint8_t retainedIndex = defaultAbilityIndex;
    const uint16_t retainedAbility = defaultAbility;
    if (nativeStarterDefaultAbility(hiddenOnly, defaultAbilityIndex, defaultAbility) ||
        defaultAbilityIndex != retainedIndex || defaultAbility != retainedAbility) return 138;
    PokemonGender defaultGender = PokemonGender::Unspecified;
    if (!nativeStarterDefaultGender(purchased, defaultGender) || defaultGender != PokemonGender::Male) return 139;
    auto femaleOnly = purchased;
    femaleOnly.genderAttr = 8;
    if (!nativeStarterDefaultGender(femaleOnly, defaultGender) || defaultGender != PokemonGender::Male) return 140;
    femaleOnly.genderAttr = 0;
    femaleOnly.abilityAttr = 0; // Legacy incomplete metadata, not a known zero-gender capture.
    if (nativeStarterDefaultGender(femaleOnly, defaultGender) || defaultGender != PokemonGender::Male) return 141;
    auto knownNoGenderBits = purchased;
    knownNoGenderBits.genderAttr = 0;
    if (!nativeStarterDefaultGender(knownNoGenderBits, defaultGender) || defaultGender != PokemonGender::Male) return 144;
    bool checkedGenderless = false;
    for (const auto& species : PokerogueContent::kSpecies) {
        if (species.malePercentTenths != 65534) continue;
        NativeStarterCandyRecord genderless{};
        genderless.speciesDex = species.dex;
        if (!nativeStarterDefaultGender(genderless, defaultGender) || defaultGender != PokemonGender::Genderless) return 142;
        checkedGenderless = true;
        break;
    }
    if (!checkedGenderless) return 143;
    purchased.costReduction = 3;
    size_t invalidWritten = 999;
    if (encodeNativeStarterCandyProfile(&purchased, 1, 1, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtEncoded, sizeof(caughtEncoded), invalidWritten) !=
            NativeSaveResult::InvalidRecord || invalidWritten) return 122;
    // A checksum-valid v6 record still rejects reserved bits and reduction three.
    const uint8_t invalidFlags[] = {8, 7};
    for (const uint8_t flags : invalidFlags) {
        caughtEncoded[88] = static_cast<char>(flags);
        char invalidDigest[65]{};
        IntegritySha256::hashHex(caughtEncoded, caughtWritten - 64, invalidDigest);
        std::memcpy(caughtEncoded + caughtWritten - 64, invalidDigest, 64);
        caughtDecoded[0].costReduction = 2;
        if (decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
                PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
                NativeSaveResult::InvalidRecord || caughtDecoded[0].costReduction != 2) return 124;
    }
    // v2 carries only caught: importing it must not invent a purchased reduction.
    std::memcpy(caughtEncoded, "P3CANDY2", 8);
    caughtEncoded[88] = 1;
    caughtWritten = kStarterCandyProfileOverhead + 9;
    char v2Digest[65]{};
    IntegritySha256::hashHex(caughtEncoded, caughtWritten - 64, v2Digest);
    std::memcpy(caughtEncoded + caughtWritten - 64, v2Digest, 64);
    if (decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || caughtDecoded[0].costReduction || !caughtDecoded[0].caught ||
        caughtDecoded[0].natureAttr) return 123;
    for (uint8_t iv : caughtDecoded[0].dexIvs) if (iv) return 129;
    // Legacy records retain candy/friendship but do not invent caught metadata.
    std::memcpy(caughtEncoded, "P3CANDY1", 8);
    --caughtWritten;
    char legacyDigest[65]{};
    IntegritySha256::hashHex(caughtEncoded, caughtWritten - 64, legacyDigest);
    std::memcpy(caughtEncoded + caughtWritten - 64, legacyDigest, 64);
    if (decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || caughtDecoded[0].caught) return 114;
    if (std::strcmp(nativeSaveResultName(NativeSaveResult::MemoryUnavailable),
            "Insufficient memory for save transaction") ||
        !std::strcmp(nativeSaveResultName(NativeSaveResult::MemoryUnavailable),
            nativeSaveResultName(NativeSaveResult::InvalidRecord))) return 116;
    for (const auto& form : PokerogueContent::kForms) {
        uint16_t speciesDex = 0;
        for (const auto& species : PokerogueContent::kSpecies)
            if (!std::strcmp(species.id, form.speciesId)) { speciesDex = species.dex; break; }
        if (!speciesDex || PokerogueContent::findFormByUpstreamIndex(speciesDex, form.upstreamFormIndex) != &form) return 145;
        PokemonActorIdentity formActor{};
        formActor.formId = form.id;
        uint64_t observed = 0;
        if (pokemonObservedDexFormAttr(speciesDex, formActor, observed) != PokemonObservedFormResult::Ok ||
            observed != (uint64_t(128) << form.upstreamFormIndex)) return 147;
        NativeStarterCandyRecord observedRecord{};
        observedRecord.speciesDex = speciesDex;
        observedRecord.caught = true;
        observedRecord.observedFormAttr = observed;
        char observedBytes[256]{};
        size_t observedSize = 0, observedCount = 0;
        uint32_t observedGeneration = 0;
        NativeStarterCandyRecord restoredObservation[1]{};
        if (encodeNativeStarterCandyProfile(&observedRecord, 1, 1, PokerogueContent::kContentHash,
                PokerogueContent::kMaxStarterCandyCount, observedBytes, sizeof(observedBytes), observedSize) != NativeSaveResult::Ok ||
            decodeNativeStarterCandyProfile(observedBytes, observedSize, PokerogueContent::kContentHash,
                PokerogueContent::kMaxStarterCandyCount, restoredObservation, 1, observedCount, observedGeneration) != NativeSaveResult::Ok ||
            restoredObservation[0].observedFormAttr != observed || observedCount != 1) return 152;

        observed = 123;
        if (pokemonObservedDexFormAttr(0, formActor, observed) != PokemonObservedFormResult::MissingSpecies ||
            observed != 123) return 148;

    }
    for (const auto& form : PokerogueContent::kForms) {
        uint16_t dex = 0;
        for (const auto& species : PokerogueContent::kSpecies) {
            if (PokerogueContent::findFormByUpstreamIndex(species.dex, form.upstreamFormIndex) == &form) {
                dex = species.dex;
                break;
            }
        }
        const PokerogueContent::FormPermission* permission = nullptr;
        for (const auto& entry : PokerogueContent::kFormPermissions)
            if (std::strcmp(entry.formId, form.id) == 0) { permission = &entry; break; }
        if (!permission) return 153;
        const auto locked = pokemonValidateStarterForm(dex, form.upstreamFormIndex, 0);
        const auto unlocked = pokemonValidateStarterForm(dex, form.upstreamFormIndex,
            uint64_t(128) << form.upstreamFormIndex);
        if (permission->isStarterSelectable == 1) {
            if (locked != PokemonStarterFormResult::NotUnlocked ||
                unlocked != PokemonStarterFormResult::Ok) return 154;
        } else if (permission->isStarterSelectable == 0) {
            if (locked != PokemonStarterFormResult::NotSelectable ||
                unlocked != PokemonStarterFormResult::NotSelectable) return 155;
        } else if (locked != PokemonStarterFormResult::UnsupportedPermission) return 156;
    }
    if (pokemonValidateStarterForm(0, 0, 0) != PokemonStarterFormResult::MissingSpecies ||
        pokemonValidateStarterForm(1, 65535, 0) != PokemonStarterFormResult::MissingForm) return 157;
    for (const auto& species : PokerogueContent::kSpecies) {
        uint64_t mask = 123;
        if (pokemonObtainableFormMask(species.dex, mask) != PokemonFormUnlockMaskResult::Ok) return 158;
        size_t count = 0;
        uint64_t expected = 0;
        for (const auto& form : PokerogueContent::kForms) {
            if (std::strcmp(form.speciesId, species.id)) continue;
            ++count;
            for (const auto& permission : PokerogueContent::kFormPermissions) {
                if (!std::strcmp(permission.formId, form.id) && permission.isUnobtainable == 0)
                    expected |= uint64_t(128) << form.upstreamFormIndex;
            }
        }
        if (mask != (count > 1 ? expected : uint64_t(128))) return 159;
    }
    uint64_t missingMask = 123;
    if (pokemonObtainableFormMask(0, missingMask) != PokemonFormUnlockMaskResult::MissingSpecies ||
        missingMask != 123) return 160;
    struct FormCaptureCase { const char* original; const char* recipient; uint16_t index; uint64_t extra; };
    const FormCaptureCase cases[] = {
        {"venusaur", "venusaur", 1, 128}, {"pikachu", "pichu", 1, 128},
        {"urshifu", "urshifu", 2, 128}, {"urshifu", "urshifu", 3, 256},
        {"urshifu", "kubfu", 3, 256},
        {"zygarde", "zygarde", 4, 512}, {"zygarde", "zygarde", 5, 1024}
    };
    for (const auto& sample : cases) {
        uint16_t original = 0, recipient = 0;
        for (const auto& species : PokerogueContent::kSpecies) {
            if (!std::strcmp(species.id, sample.original)) original = species.dex;
            if (!std::strcmp(species.id, sample.recipient)) recipient = species.dex;
        }
        const auto* form = PokerogueContent::findFormByUpstreamIndex(original, sample.index);
        if (!form) return 164;
        PokemonActorIdentity actor{};
        actor.formId = form->id;
        uint64_t unlocked = 0, allowed = 0;
        if (pokemonObtainableFormMask(recipient, allowed) != PokemonFormUnlockMaskResult::Ok ||
            pokemonCaptureFormUnlocks(original, actor, recipient, unlocked) != PokemonCaptureFormUnlockResult::Ok ||
            unlocked != (((uint64_t(128) << sample.index) & allowed) | sample.extra)) return 165;
        NativeStarterCandyRecord durable{};
        durable.speciesDex = recipient;
        durable.caught = true;
        durable.unlockedFormAttr = unlocked;
        char bytes[256]{};
        size_t length = 0, count = 0;
        uint32_t generation = 0;
        NativeStarterCandyRecord restored[1]{};
        if (encodeNativeStarterCandyProfile(&durable, 1, 1, PokerogueContent::kContentHash,
                PokerogueContent::kMaxStarterCandyCount, bytes, sizeof(bytes), length) != NativeSaveResult::Ok ||
            decodeNativeStarterCandyProfile(bytes, length, PokerogueContent::kContentHash,
                PokerogueContent::kMaxStarterCandyCount, restored, 1, count, generation) != NativeSaveResult::Ok ||
            restored[0].unlockedFormAttr != unlocked) return 168;
        if (!std::strcmp(sample.recipient, "kubfu")) {
            durable.observedFormAttr = 256;
            if (StarterCandyProfileCodec::valid(durable, 0, PokerogueContent::kMaxStarterCandyCount) ||
                pokemonValidateStarterForm(recipient, 1, unlocked) != PokemonStarterFormResult::MissingForm) return 169;
        }

    }
    uint64_t failedUnlock = 123;
    PokemonActorIdentity noActor{};
    if (pokemonCaptureFormUnlocks(0, noActor, 1, failedUnlock) != PokemonCaptureFormUnlockResult::InvalidSpecies ||
        failedUnlock != 123) return 166;
    for (const auto& species : PokerogueContent::kSpecies) {
        NativeStarterCandyRecord record{};
        record.speciesDex = species.dex;
        record.genderAttr = 8; // Only a female observed must not override species default.
        record.abilityAttr = 1;
        record.natureAttr = 2;
        PokemonGender actual = PokemonGender::Unspecified;
        const PokemonGender expected = species.malePercentTenths == 65534 ? PokemonGender::Genderless :
            species.malePercentTenths == 0 ? PokemonGender::Female : PokemonGender::Male;
        if (!nativeStarterDefaultGender(record, actual) || actual != expected) return 167;
    }
    purchased.costReduction = 2; // Restore valid input after the reduction-three rejection case.
    char v7Bytes[256]{};
    const size_t v7Size = kStarterCandyProfileOverhead + 37;
    char currentPreferenceBytes[256]{};
    size_t currentPreferenceSize = 0;
    if (encodeNativeStarterCandyProfile(&purchased, 1, 1, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, currentPreferenceBytes, sizeof(currentPreferenceBytes),
            currentPreferenceSize) != NativeSaveResult::Ok) return 174;
    std::memcpy(v7Bytes, currentPreferenceBytes, 117);
    std::memcpy(v7Bytes, "P3CANDY7", 8);
    char v7Digest[65]{};
    IntegritySha256::hashHex(v7Bytes, v7Size - 64, v7Digest);
    std::memcpy(v7Bytes + v7Size - 64, v7Digest, 64);
    NativeStarterCandyRecord v7Decoded[1]{};
    if (decodeNativeStarterCandyProfile(v7Bytes, v7Size, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, v7Decoded, 1, caughtCount, caughtGeneration) != NativeSaveResult::Ok ||
        v7Decoded[0].unlockedFormAttr != purchased.unlockedFormAttr || v7Decoded[0].preferredFormIndex != 65535) return 170;
    size_t selectablePreferences = 0;
    for (const auto& species : PokerogueContent::kSpecies) {
        if (!species.starterEligible) continue;
        for (const auto& form : PokerogueContent::kForms) {
            if (std::strcmp(form.speciesId, species.id) || form.upstreamFormIndex > 56) continue;
            const uint64_t bit = uint64_t(128) << form.upstreamFormIndex;
            if (pokemonValidateStarterForm(species.dex, form.upstreamFormIndex, bit) != PokemonStarterFormResult::Ok) continue;
            NativeStarterCandyRecord preference{};
            preference.speciesDex = species.dex;
            preference.caught = true;
            preference.unlockedFormAttr = bit;
            preference.preferredFormIndex = form.upstreamFormIndex;
            char bytes[256]{};
            size_t length = 0, count = 0;
            uint32_t generation = 0;
            NativeStarterCandyRecord restored[1]{};
            if (encodeNativeStarterCandyProfile(&preference, 1, 1, PokerogueContent::kContentHash,
                    PokerogueContent::kMaxStarterCandyCount, bytes, sizeof(bytes), length) != NativeSaveResult::Ok ||
                decodeNativeStarterCandyProfile(bytes, length, PokerogueContent::kContentHash,
                    PokerogueContent::kMaxStarterCandyCount, restored, 1, count, generation) != NativeSaveResult::Ok ||
                restored[0].preferredFormIndex != preference.preferredFormIndex) return 171;
            preference.unlockedFormAttr = 0;
            if (StarterCandyProfileCodec::valid(preference, 0, PokerogueContent::kMaxStarterCandyCount)) return 172;
            ++selectablePreferences;
        }
    }
    if (!selectablePreferences) return 173;
    if (PokerogueContent::findFormByUpstreamIndex(0, 0)) return 146;
    return 0;
}
