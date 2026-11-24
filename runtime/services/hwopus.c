// hwopus: the Switch's hardware Opus decoder (IHardwareOpusDecoderManager / IHardwareOpusDecoder), with libopus -
// the reference decoder, which the hardware service wraps too (linked statically: no library to ship).
//
// Scarlet / Violet's sound engine asks for the decoder's work buffer size at boot (GetWorkBufferSizeEx) and opens
// decoders for what it streams in Opus: the title screen's sound (a 48 kHz and a 24 kHz stereo stream). With no
// service here (a logging stub answering zeros) it opened none, and the title screen was silent.
//
// Requests: the decoder's input is one packet - a header (the packet's size and final range, big-endian) and its
// bytes - and its output 16-bit samples, interleaved; the reply is the bytes consumed (header included), the samples
// per channel and, for the *WithPerf variants, the time it took (us). A multistream decoder's packet is the streams'
// packets in libopus' multistream layout, with the channel mapping given at open. The "Ex" parameters add a flag
// for 40 / 60 ms frames, which libopus decodes as they come. LARECOMP_HWOPUS=0: the stub again (silent).
#include "../hle.h"
#include "../ipc.h"
#include "../rt.h"
#include <opus/opus.h>
#include <opus/opus_multistream.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    OpusDecoder *dec;
    OpusMSDecoder *ms;
    uint32_t rate, channels;
} Opus;

static bool hwopus_on(void) {
    static int on = -1;
    if (on < 0) on = !getenv("LARECOMP_HWOPUS") || atoi(getenv("LARECOMP_HWOPUS")) != 0;
    return on;
}

static bool rate_ok(uint32_t r) { return r == 48000 || r == 24000 || r == 16000 || r == 12000 || r == 8000; }

// (the game allocates it and hands it over as transfer memory: libopus keeps its state in its own memory)
static uint32_t work_size(uint32_t channels, uint32_t streams) {
    if (!channels || channels > 255) channels = 2;
    if (!streams || streams > 255) streams = 1;
    return 0x4000 + 0x4000 * channels + 0x8000 * streams;
}

static Opus *opus_open(uint32_t rate, uint32_t channels, uint32_t streams, uint32_t coupled, const uint8_t *mapping) {
    if (!rate_ok(rate) || !channels || channels > 255) return NULL;
    Opus *o = calloc(1, sizeof *o);
    if (!o) return NULL;
    o->rate = rate, o->channels = channels;
    int err = OPUS_OK;
    if (mapping) o->ms = opus_multistream_decoder_create((opus_int32)rate, (int)channels, (int)streams, (int)coupled, mapping, &err);
    else if (channels <= 2) o->dec = opus_decoder_create((opus_int32)rate, (int)channels, &err);
    else err = OPUS_BAD_ARG;
    if (err != OPUS_OK || (!o->dec && !o->ms)) {
        LOG("[hwopus] libopus didn't open %u Hz, %u channels, %u streams: %s\n", rate, channels, streams, opus_strerror(err));
        free(o);
        return NULL;
    }
    return o;
}

static void opus_close(Service *s) {
    Opus *o = s->state;
    if (!o) return;
    if (o->dec) opus_decoder_destroy(o->dec);
    if (o->ms) opus_multistream_decoder_destroy(o->ms);
    free(o);
    s->state = NULL;
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

// One packet from in into out (s16 interleaved). false: not a packet, or it didn't decode (*consumed: what to skip).
static bool opus_decode_packet(Opus *o, const IpcBuf *in, const IpcBuf *out, bool reset, uint32_t *consumed, uint32_t *samples) {
    *consumed = *samples = 0;
    if (!in || in->size < 8) return false;
    const uint8_t *src = (const uint8_t *)(uintptr_t)in->addr;
    uint32_t size = be32(src);
    if (size > in->size - 8) { *consumed = (uint32_t)in->size; return false; }
    *consumed = 8 + size;
    if (!o || !out || !out->size) return false;
    if (reset) {
        if (o->dec) opus_decoder_ctl(o->dec, OPUS_RESET_STATE);
        if (o->ms) opus_multistream_decoder_ctl(o->ms, OPUS_RESET_STATE);
    }
    int frames = (int)(out->size / (2ull * o->channels));
    opus_int16 *dst = (opus_int16 *)(uintptr_t)out->addr;
    int n = o->ms ? opus_multistream_decode(o->ms, src + 8, (opus_int32)size, dst, frames, 0)
                  : opus_decode(o->dec, src + 8, (opus_int32)size, dst, frames, 0);
    if (n < 0) {
        static int nlog;
        if (nlog++ < 10) LOG("[hwopus] a packet of %u bytes didn't decode: %s\n", size, opus_strerror(n));
        return false;
    }
    *samples = (uint32_t)n;
    return true;
}

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static void hw_opus_decoder(Service *s, IpcReq *req, IpcResp *resp) {
    Opus *o = s->state;
    switch (req->cmd) {
    case 0: case 2: case 4: case 5: case 6: case 7: case 8: case 9: {
        // DecodeInterleaved[ForMultiStream]: Old (0, 2), WithPerfOld (4, 5), WithPerfAndResetOld (6, 7), current (8, 9)
        bool reset = req->cmd >= 6 && req->data_size >= 1 && IN(uint8_t, 0) != 0;
        uint64_t t0 = now_us();
        uint32_t consumed, samples;
        // (a packet that doesn't decode is consumed with no samples, not an error code the game may stop on)
        opus_decode_packet(o, in_buf(req, 0), out_buf(req, 0), reset, &consumed, &samples);
        OUT(uint32_t, 0) = consumed;
        OUT(uint32_t, 4) = samples;
        resp_size(resp, 8);
        if (req->cmd >= 4) { OUT(uint64_t, 8) = now_us() - t0; resp_size(resp, 16); }
        break;
    }
    case 1: case 3: break;  // SetContext[ForMultiStream]: the decoder keeps its own state
    default: LOG("[hwopus] decoder cmd %u (ok)\n", req->cmd);
    }
}

// (a format libopus can't open still gets a decoder: its packets decode to nothing, the stub's silence)
static void open_decoder(IpcResp *resp, Opus *o) {
    Service *d = service_new("IHardwareOpusDecoder", hw_opus_decoder, o);
    d->on_close = opus_close;
    ipc_return_object(resp, d);
}

// a multistream request's parameters (a buffer): rate, channels, streams, coupled streams[, Ex: flag + 7 pad], mapping
static Opus *open_multi(IpcReq *req, bool ex) {
    IpcBuf *b = in_buf(req, 0);
    if (!b || b->size < 16) return NULL;
    const uint8_t *p = (const uint8_t *)(uintptr_t)b->addr;
    uint32_t rate, ch, st, cp;
    memcpy(&rate, p, 4), memcpy(&ch, p + 4, 4), memcpy(&st, p + 8, 4), memcpy(&cp, p + 12, 4);
    uint32_t moff = ex ? 24 : 16;
    if (ch > 255 || b->size < moff + ch) return NULL;
    return opus_open(rate, ch, st, cp, p + moff);
}

SERVICE("hwopus", hwopus) {
    (void)s;
    if (!hwopus_on()) { LOG("[hwopus] cmd %u (off)\n", req->cmd); return; }
    switch (req->cmd) {
    case 0: case 4: {  // OpenHardwareOpusDecoder[Ex](rate, channels[, flags], work size, transfer memory)
        Opus *o = opus_open(IN(uint32_t, 0), IN(uint32_t, 4), 1, 0, NULL);
        LOG("[hwopus] decoder %u Hz, %u channels%s\n", IN(uint32_t, 0), IN(uint32_t, 4), o ? "" : ": not opened");
        open_decoder(resp, o);
        break;
    }
    case 1: case 5: case 8:  // GetWorkBufferSize[Ex[Ex]](rate, channels[, flags])
        OUT(uint32_t, 0) = work_size(IN(uint32_t, 4), 1);
        resp_size(resp, 4);
        break;
    case 2: case 6: {  // OpenHardwareOpusDecoderForMultiStream[Ex](parameters buffer, work size, transfer memory)
        Opus *o = open_multi(req, req->cmd == 6);
        LOG("[hwopus] multistream decoder%s\n", o ? "" : ": not opened");
        open_decoder(resp, o);
        break;
    }
    case 3: case 7: case 9: {  // GetWorkBufferSizeForMultiStream[Ex[Ex]](parameters buffer)
        IpcBuf *b = in_buf(req, 0);
        uint32_t ch = 2, st = 1;
        if (b && b->size >= 12) memcpy(&ch, (const uint8_t *)(uintptr_t)b->addr + 4, 4), memcpy(&st, (const uint8_t *)(uintptr_t)b->addr + 8, 4);
        OUT(uint32_t, 0) = work_size(ch, st);
        resp_size(resp, 4);
        break;
    }
    default: LOG("[hwopus] cmd %u (ok)\n", req->cmd);
    }
}
