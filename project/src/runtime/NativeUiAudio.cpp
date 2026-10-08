#include "runtime/NativeUiAudio.hpp"
#include <cstdio>
#include <cstring>

namespace Pokerogue3DS {
bool NativeUiAudio::init() {
    fini();
    m_error=nullptr;
    m_result=ndspInit();
    if(R_FAILED(m_result)) {m_error="NDSP initialization failed";return false;}
    m_dspInitialized=true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    for(size_t i=0;i<kCount;++i) {
        const auto& definition=kNativeUiSounds[i];
        if(!definition.frames || definition.bytes!=definition.frames*4u
            || definition.bytes>128u*1024u || definition.rate<8000 || definition.rate>48000) {
            m_error="Invalid imported UI sound geometry";fini();return false;
        }
        FILE* file=std::fopen(definition.path,"rb");
        if(!file) {m_error="Imported UI sound missing from RomFS";fini();return false;}
        auto& clip=m_clips[i];
        clip.pcm=linearAlloc(definition.bytes);
        const bool complete=clip.pcm
            && std::fread(clip.pcm,1,definition.bytes,file)==definition.bytes
            && std::fgetc(file)==EOF && !std::ferror(file);
        std::fclose(file);
        if(!complete) {m_error=clip.pcm ? "Invalid UI PCM file length" : "UI audio linear allocation failed";fini();return false;}
        DSP_FlushDataCache(clip.pcm,definition.bytes);
        clip.buffer.data_vaddr=clip.pcm;
        clip.buffer.nsamples=definition.frames;
    }
    m_ready=true;
    return true;
}
void NativeUiAudio::fini() {
    m_ready=false;
    // Stop the DSP before releasing buffers it may still be reading.
    if(m_dspInitialized) {ndspChnWaveBufClear(0);ndspExit();m_dspInitialized=false;}
    for(auto& clip:m_clips) {
        if(clip.pcm) linearFree(clip.pcm);
        clip={};
    }
}
bool NativeUiAudio::setVolumes(unsigned master,unsigned ui) {
    if(master>kAudioVolumeMax || ui>kAudioVolumeMax) return false;
    m_masterVolume=master;m_uiVolume=ui;
    if(m_ready) {
        float mix[12]{};mix[0]=mix[1]=float(master*ui)/100.0f;
        ndspChnSetMix(0,mix);
    }
    return true;
}
bool NativeUiAudio::play(const char* key) {
    if(!m_ready || !key) return false;
    for(size_t i=0;i<kCount;++i) if(!std::strcmp(key,kNativeUiSounds[i].key)) {
        ndspChnWaveBufClear(0);
        ndspChnReset(0);
        ndspChnSetInterp(0,NDSP_INTERP_NONE);
        ndspChnSetRate(0,static_cast<float>(kNativeUiSounds[i].rate));
        ndspChnSetFormat(0,NDSP_FORMAT_STEREO_PCM16);
        float mix[12]{};mix[0]=mix[1]=float(m_masterVolume*m_uiVolume)/100.0f;
        ndspChnSetMix(0,mix);
        auto& buffer=m_clips[i].buffer;
        buffer.status=NDSP_WBUF_FREE;
        buffer.next=nullptr;
        ndspChnWaveBufAdd(0,&buffer);
        return true;
    }
    return false;
}
}
