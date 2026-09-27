# Testing ImsMedia

Run the Android suites in a complete product checkout with the paired ImsStack
and media fork. Use a connected device or Cuttlefish instance appropriate to the
features being tested.

```sh
m ImsMediaService libimsmedia ImsMediaJavaUnitTests ImsMediaNativeTests
atest ImsMediaJavaUnitTests ImsMediaNativeTests
```

`ImsMediaJavaUnitTests` covers framework/service behavior. `ImsMediaNativeTests` is the
aggregate native suite; several smaller native modules are disabled in the build,
so use the aggregate target unless deliberately enabling a component module.

Focused native examples:

```sh
atest ImsMediaNativeTests:ImsMediaBitReaderTest
atest ImsMediaNativeTests:ImsMediaImageRotateTest
atest ImsMediaNativeTests:IImsMediaThreadTest
atest ImsMediaNativeTests:StreamSchedulerTest
```

The worker tests ensure that shutdown waits for the worker, duplicate start
requests cannot spawn another worker, and reuse waits for the previous worker to
return. Utility tests cover invalid/truncated input, buffer boundaries, image
layouts and preserved output guards. Host runs can exercise these pure utilities
with Android logging/scheduling shims; they do not validate Binder, codecs or HALs.

## Reproducible host checks

Use clang++, Python 3 and a GoogleTest v1.17.0 source checkout. Pass the
`googletest` subdirectory of that checkout to each runner:

```sh
python3 tests/host/run_utility_tests.py \
    --googletest /path/to/googletest/googletest \
    --output-dir /path/to/temp/media-utilities
python3 tests/host/run_audio_recovery_tests.py \
    --googletest /path/to/googletest/googletest \
    --output-dir /path/to/temp/media-recovery
```

Both runners build the production implementation with ASan/UBSan and keep every
generated header/binary in the selected output directory. The utility/worker
tests replace Android logging and scheduling priority plumbing. The playback
tests use a controlled AAudio/timer backend and L16 frames; media codec entrypoints
are link-only stubs. They cover recovery after a failed reopen, retry pacing,
stop, stale callbacks and failed-initial-start behavior. These checks do not
establish HAL or codec compatibility.

## Device coverage

For a device regression pass, exercise:

- AMR-NB/WB calls, silence/DTX, packet loss and resumed speech.
- Capture/playback startup failures and audio-route disconnect/recovery.
- Earpiece, speaker, wired and Bluetooth paths, including preprocessing effects.
- Early hangup, repeated call start/stop, DTMF and long calls.
- Video camera and pause-image startup failure, source replacement and shutdown.
- RTP/RTCP socket handoff and the product's SELinux policy.

Check both normal playback and the optional low-latency path on products that
enable it. EVS codec operation is unsupported. Validate the Android 16 product
with the paired Android 17 media API separately from the native Android 17 build.

Run the platform suites and device regression pass for each release. Record the
product, source revisions, routing configuration and results alongside that
release's test report so host and device coverage can be assessed separately.
