# NRI Video Tests

Configure a release test build:

```bash
cmake -S . -B _BuildVideo -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DNRI_BUILD_TESTS=ON
cmake --build _BuildVideo -j
```

Run only the deterministic CPU suite:

```bash
ctest --test-dir _BuildVideo -L '^video-short$' --output-on-failure
```

Run the overnight fuzz, stress, and soak defaults explicitly:

```bash
cmake --build _BuildVideo --target NRI_VideoLongTests
```

Use small slices while developing:

```bash
cmake -S . -B _BuildVideo -DNRI_VIDEO_REGISTER_LONG_TESTS=ON
cmake --build _BuildVideo -j
NRI_VIDEO_TEST_ITERATIONS=64 NRI_VIDEO_TEST_DURATION_MS=2 \
    ctest --test-dir _BuildVideo -L '^video-(fuzz|stress|soak)$' --output-on-failure
```

Enable hardware tests separately. `NRI_VIDEO_REQUIRE_FULL_CODEC_MATRIX=ON` requires H.264, H.265, and AV1 for both decode and encode; set it to `OFF` for heterogeneous CI hardware.

```bash
cmake -S . -B _BuildVideoHardware \
    -DCMAKE_BUILD_TYPE=Release \
    -DNRI_BUILD_TESTS=ON \
    -DNRI_BUILD_VIDEO_HARDWARE_TESTS=ON
cmake --build _BuildVideoHardware -j
ctest --test-dir _BuildVideoHardware -L '^video-hardware-short$' --output-on-failure
```

For Clang/GCC sanitizer coverage:

```bash
cmake -S . -B _BuildVideoSan -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DNRI_BUILD_TESTS=ON \
    -DNRI_VIDEO_ENABLE_SANITIZERS=ON
cmake --build _BuildVideoSan -j
ctest --test-dir _BuildVideoSan -L '^video-short$' --output-on-failure
```
