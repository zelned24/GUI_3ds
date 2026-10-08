#pragma once
#include "content/AudioVolumePolicy.hpp"
#include "content/NativeUiSounds.hpp"
#include <3ds.h>
#include <cstddef>

namespace Pokerogue3DS {
// NDSP owner for the three imported UI sounds. Music uses a separate future stream.
class NativeUiAudio {
public:
    NativeUiAudio() = default;
    NativeUiAudio(const NativeUiAudio&) = delete;
    NativeUiAudio& operator=(const NativeUiAudio&) = delete;
    ~NativeUiAudio() { fini(); }
    bool init();
    void fini();
    bool play(const char* key);
    bool setVolumes(unsigned master,unsigned ui);
    bool ready() const { return m_ready; }
    Result initializationResult() const { return m_result; }
    const char* initializationError() const { return m_error; }
private:
    static constexpr size_t kCount=sizeof(kNativeUiSounds)/sizeof(kNativeUiSounds[0]);
    struct Clip {void* pcm=nullptr;ndspWaveBuf buffer{};};
    Clip m_clips[kCount]{};
    bool m_dspInitialized=false;
    bool m_ready=false;
    unsigned m_masterVolume=kDefaultMasterVolume,m_uiVolume=kDefaultUiVolume;
    Result m_result=0;
    const char* m_error=nullptr;
};
}
