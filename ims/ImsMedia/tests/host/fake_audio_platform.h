// SPDX-License-Identifier: Apache-2.0
// Minimal controllable platform backend for the real ImsMediaAudioPlayer L16 path.
#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <sys/types.h>
using aaudio_result_t = int32_t;
using aaudio_stream_state_t = int32_t;
using aaudio_sharing_mode_t = int32_t;
using aaudio_performance_mode_t = int32_t;
constexpr int AAUDIO_OK = 0;
constexpr int AAUDIO_ERROR_DISCONNECTED = -899;
constexpr int AAUDIO_ERROR_UNAVAILABLE = -889;
constexpr int AAUDIO_STREAM_STATE_UNINITIALIZED = 0;
constexpr int AAUDIO_STREAM_STATE_STARTING = 3;
constexpr int AAUDIO_STREAM_STATE_STARTED = 4;
constexpr int AAUDIO_STREAM_STATE_STOPPING = 9;
constexpr int AAUDIO_SHARING_MODE_EXCLUSIVE = 0;
constexpr int AAUDIO_SHARING_MODE_SHARED = 1;
constexpr int AAUDIO_PERFORMANCE_MODE_NONE = 10;
constexpr int AAUDIO_PERFORMANCE_MODE_LOW_LATENCY = 12;
constexpr int AAUDIO_CONTENT_TYPE_SPEECH = 1;
constexpr int AAUDIO_DIRECTION_OUTPUT = 0;
constexpr int AAUDIO_FORMAT_PCM_I16 = 1;
constexpr int AAUDIO_USAGE_VOICE_COMMUNICATION = 2;
struct AAudioStream;
using ErrorCallback = void (*)(AAudioStream*, void*, aaudio_result_t);
struct AAudioStreamBuilder { ErrorCallback error = nullptr; void* user = nullptr; };
struct AAudioStream { ErrorCallback error; void* user; int state; bool closed = false; };
namespace FakeAudio {
inline uint64_t nowUs = 1000000;
inline int openAttempts = 0, writes = 0;
inline int failOpen = 0, failStart = 0, failWait = 0;
inline std::vector<std::unique_ptr<AAudioStream>> streams;
inline AAudioStream* current() { return streams.empty() ? nullptr : streams.back().get(); }
inline void reset() {
    streams.clear(); nowUs = 1000000; openAttempts = writes = 0;
    failOpen = failStart = failWait = 0;
}
inline void disconnect() { auto* stream = current(); stream->error(stream, stream->user, AAUDIO_ERROR_DISCONNECTED); }
}
inline aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder** b) { *b = new AAudioStreamBuilder; return 0; }
inline aaudio_result_t AAudioStreamBuilder_delete(AAudioStreamBuilder* b) { delete b; return 0; }
inline void AAudioStreamBuilder_setErrorCallback(AAudioStreamBuilder* b, ErrorCallback f, void* u) { b->error = f; b->user = u; }
inline aaudio_result_t AAudioStreamBuilder_openStream(AAudioStreamBuilder* b, AAudioStream** out) {
    ++FakeAudio::openAttempts;
    if (FakeAudio::failOpen > 0) { --FakeAudio::failOpen; *out = nullptr; return AAUDIO_ERROR_UNAVAILABLE; }
    FakeAudio::streams.push_back(std::make_unique<AAudioStream>(AAudioStream{b->error, b->user, AAUDIO_STREAM_STATE_UNINITIALIZED}));
    *out = FakeAudio::current(); return AAUDIO_OK;
}
inline aaudio_result_t AAudioStream_requestStart(AAudioStream* s) {
    if (FakeAudio::failStart > 0) { --FakeAudio::failStart; return AAUDIO_ERROR_UNAVAILABLE; }
    s->state = AAUDIO_STREAM_STATE_STARTED; return AAUDIO_OK;
}
inline aaudio_result_t AAudioStream_requestStop(AAudioStream* s) { s->state = AAUDIO_STREAM_STATE_STOPPING; return 0; }
inline aaudio_result_t AAudioStream_close(AAudioStream* s) { s->closed = true; return 0; }
inline aaudio_stream_state_t AAudioStream_getState(AAudioStream* s) { return s->state; }
inline aaudio_result_t AAudioStream_waitForStateChange(AAudioStream* s, int, int* next, int64_t) {
    if (FakeAudio::failWait > 0) { --FakeAudio::failWait; *next = AAUDIO_STREAM_STATE_STARTING; return AAUDIO_ERROR_UNAVAILABLE; }
    *next = s->state; return 0;
}
inline aaudio_result_t AAudioStream_write(AAudioStream*, const void*, int32_t frames, int64_t) { ++FakeAudio::writes; return frames; }
inline int AAudioStream_getPerformanceMode(AAudioStream*) { return AAUDIO_PERFORMANCE_MODE_NONE; }
inline const char* AAudio_convertResultToText(int) { return "fake"; }
inline const char* AAudio_convertStreamStateToText(int) { return "fake"; }
#define FAKE_BUILDER_SETTER(name) inline void AAudioStreamBuilder_##name(AAudioStreamBuilder*, int32_t) {}
FAKE_BUILDER_SETTER(setChannelCount)
FAKE_BUILDER_SETTER(setContentType)
FAKE_BUILDER_SETTER(setDirection)
FAKE_BUILDER_SETTER(setFormat)
FAKE_BUILDER_SETTER(setPerformanceMode)
FAKE_BUILDER_SETTER(setSampleRate)
FAKE_BUILDER_SETTER(setSharingMode)
FAKE_BUILDER_SETTER(setUsage)
#undef FAKE_BUILDER_SETTER
inline bool property_get_bool(const char*, bool fallback) { return fallback; }
namespace android { template <typename T> class sp; }
class ImsMediaTimer { public: static uint64_t GetTimeInMicroSeconds() { return FakeAudio::nowUs; } };
// The tests use L16, so no media codec is created or decoded. These declarations
// let the complete production translation unit link without Android libraries.
using media_status_t = int32_t;
struct AMediaCodec {};
struct AMediaFormat {};
struct AMediaCodecBufferInfo { int32_t offset, size; int64_t presentationTimeUs; uint32_t flags; };
constexpr int AMEDIA_OK = 0;
constexpr int AMEDIACODEC_INFO_TRY_AGAIN_LATER = -1;
constexpr int AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED = -2;
constexpr int AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED = -3;
inline const char* AMEDIAFORMAT_KEY_MIME = "mime";
inline const char* AMEDIAFORMAT_KEY_CHANNEL_COUNT = "channel-count";
inline const char* AMEDIAFORMAT_KEY_SAMPLE_RATE = "sample-rate";
inline AMediaCodec* AMediaCodec_createDecoderByType(...) { return nullptr; }
inline AMediaFormat* AMediaFormat_new() { return nullptr; }
inline AMediaFormat* AMediaCodec_getOutputFormat(...) { return nullptr; }
inline uint8_t* AMediaCodec_getInputBuffer(...) { return nullptr; }
inline uint8_t* AMediaCodec_getOutputBuffer(...) { return nullptr; }
inline const char* AMediaFormat_toString(...) { return "unused"; }
inline ssize_t AMediaCodec_dequeueInputBuffer(...) { return -1; }
inline ssize_t AMediaCodec_dequeueOutputBuffer(...) { return -1; }
#define FAKE_CODEC_FUNCTION(name) inline media_status_t name(...) { return AMEDIA_OK; }
FAKE_CODEC_FUNCTION(AMediaCodec_configure)
FAKE_CODEC_FUNCTION(AMediaCodec_delete)
FAKE_CODEC_FUNCTION(AMediaCodec_queueInputBuffer)
FAKE_CODEC_FUNCTION(AMediaCodec_releaseOutputBuffer)
FAKE_CODEC_FUNCTION(AMediaCodec_start)
FAKE_CODEC_FUNCTION(AMediaCodec_stop)
FAKE_CODEC_FUNCTION(AMediaFormat_delete)
#undef FAKE_CODEC_FUNCTION
inline void AMediaFormat_setString(...) {}
inline void AMediaFormat_setInt32(...) {}
