# ImsMedia for the paired ImsStack fork

ImsMedia provides RTP/RTCP, media codecs and Android audio/video integration for
the paired ImsStack fork. The service is `com.android.telephony.imsmedia`, runs
under the phone UID, and receives RTP/RTCP sockets from ImsStack over Binder.
Carrier selection and SIP policy belong in ImsStack/CarrierSettings.

This fork is based on AOSP `android17-release` at
`0101cb4caf48dbcc3b8fbc6ab3e41137b90b8e67`. The public Java/AIDL media interface
is unchanged by this fork. Android 16 products must use this paired media API
with ImsStack's Android 16 telephony compatibility option.

## Product integration

Check out at `packages/modules/ImsMedia`, then include:

```make
$(call inherit-product, packages/modules/ImsMedia/imsmedia.mk)
```

This installs `ImsMediaService`, `libimsmedia` and the user-installation allowlist.
Add to BoardConfig:

```make
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += packages/modules/ImsMedia/sepolicy/system_ext/private
```

The downstream policy permits media access to platform-app UDP sockets passed
by ImsStack and lookup of SchedulingPolicyService. Validate the actual domains,
audio HAL and policy on each product. The fork does not set device-wide audio
routing, APNs, carrier service availability or QNS/IWLAN policy.

Build with:

```sh
m ImsMediaService libimsmedia ImsMediaJavaUnitTests ImsMediaNativeTests
```

## Audio behavior and limits

- Playback defaults to AAudio's normal performance mode for compatibility with
  devices whose MMAP route produces no audible downlink. A validated product can
  opt in with `persist.radio.imsmedia.low_latency_playback=true`.
- Capture requests voice communication, normal performance and an allocated
  effect-capable session so the audio policy can attach preprocessing effects.
  This does not establish that echo cancellation/noise suppression actually works
  on every device; verify the selected HAL path and audible result.
- Stream startup failures propagate to the media graph. Failed or disconnected
  capture streams are reopened while their session remains active. Playback also
  retries a failed reopen on subsequent frames, at least 100 ms between attempts.
  Stopping the session cancels recovery; stale stream callbacks are ignored.
- AMR DTX keeps playback fed without replacing decoder-generated comfort noise.
  Lost active-speech frames use the AMR decoder's concealment path.
- EVS parameters remain in the public API, but the open implementation has no
  working EVS encoder/decoder. The audio implementation rejects that path and the
  paired ImsStack prevents EVS negotiation.

The audio implementation includes shared-mode fallback, bounded stream operations,
codec buffer validation and serialized teardown. Device-specific workarounds must
stay explicit and preserve an opt-in/default distinction where appropriate.

## Worker, payload and video behavior

The implementation validates bitstreams, Base16/Base64 conversion,
audio/T.140/video RTP payloads, YUV dimensions and codec buffer ranges. The bit
reader rejects oversized reads without corrupting its state, and image rotation
helpers reject empty or invalid layouts. Video source/renderer workers have
owned lifetimes and startup rollback.

Media workers are joined before resources are released or a stopped worker is
reused. Stop flags and timed exit notifications alone do not prove a thread has
returned. Scheduler, capture, playback, DTMF, quality analysis and event-handler
shutdown use this rule. Event handlers release their queue lock before joining
a callback worker.

A worker stuck inside a platform/HAL call can still delay teardown. Freeing its
resources while it runs is not a valid recovery mechanism; validate route changes,
stop/restart and platform recovery on the target device.

## Validation

The host utility runner builds production binary-format, bit-reader, bit-writer,
image-rotation and worker-lifetime code with address and undefined-behavior
sanitizers. It replaces Android logging and scheduling-priority plumbing for host
execution. The playback runner exercises the production player with a controlled
AAudio/timer backend, covering failed reopen/start/state transitions, cancellation,
stale callbacks and initial-start boundaries.

These checks do not exercise Android codecs or a real HAL. Validate full Android
compilation, device audio/video, Bluetooth/wired/speaker routing, preprocessing
effects, live RTP loss and long-call stability on each supported product. See
[tests/README.md](tests/README.md) for reproducible host commands, platform suites
and the device regression matrix.
