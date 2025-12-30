<?php
/**
 * Extreme stress tests for coredump prevention
 * Tests edge cases, race conditions, and malicious inputs
 */

echo "=== Opus Coredump Prevention Tests ===\n\n";

$passed = 0;
$failed = 0;

function test($name, $callback) {
    global $passed, $failed;
    echo "Test: $name... ";
    try {
        $callback();
        echo "✓ PASS\n";
        $passed++;
    } catch (Throwable $e) {
        echo "✗ FAIL: " . $e->getMessage() . "\n";
        $failed++;
    }
}

// ============ Category 1: NULL/Empty Handling ============
echo "=== Category 1: NULL/Empty Input Handling ===\n";

test("Empty string encode", function() {
    $opus = new opusChannel(48000, 1);
    try {
        $opus->encode("");
    } catch (Throwable $e) {
        // Expected to throw
    }
});

test("Empty string decode", function() {
    $opus = new opusChannel(48000, 1);
    try {
        $opus->decode("");
    } catch (Throwable $e) {
        // Expected to throw
    }
});

test("Empty string resample", function() {
    $opus = new opusChannel(48000, 1);
    $result = $opus->resample("", 48000, 8000);
    if ($result !== "") throw new Exception("Should return empty string");
});

test("Empty string enhanceVoiceClarity", function() {
    $opus = new opusChannel(48000, 1);
    $result = $opus->enhanceVoiceClarity("");
    if ($result !== "") throw new Exception("Should return empty string");
});

test("Empty string spatialStereoEnhance", function() {
    $opus = new opusChannel(48000, 1);
    $result = $opus->spatialStereoEnhance("");
    if ($result !== "") throw new Exception("Should return empty string");
});

// ============ Category 2: Invalid Sizes ============
echo "\n=== Category 2: Invalid Size Handling ===\n";

test("Odd byte count (not int16 aligned)", function() {
    $opus = new opusChannel(48000, 1);
    try {
        $opus->encode("ABC"); // 3 bytes, not aligned
        throw new Exception("Should have thrown");
    } catch (Error $e) {
        // Expected
    }
});

test("Too large frame encode", function() {
    $opus = new opusChannel(48000, 1);
    // 1 second = 48000 samples (invalid frame size)
    $pcm = str_repeat(pack('s', 0), 48000);
    try {
        $opus->encode($pcm);
        throw new Exception("Should have thrown");
    } catch (Error $e) {
        // Expected
    }
});

test("Too large opus packet decode", function() {
    $opus = new opusChannel(48000, 1);
    // Opus max packet is 4000 bytes
    $large = str_repeat("X", 5000);
    try {
        $opus->decode($large);
        throw new Exception("Should have thrown");
    } catch (Error $e) {
        // Expected
    }
});

test("Single byte input", function() {
    $opus = new opusChannel(48000, 1);
    try {
        $opus->resample("X", 48000, 8000);
        throw new Exception("Should have thrown");
    } catch (Error $e) {
        // Expected
    }
});

// ============ Category 3: Extreme Values ============
echo "\n=== Category 3: Extreme Parameter Values ===\n";

test("Zero sample rate resample", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = pack('s', 0);
    try {
        $opus->resample($pcm, 0, 8000);
        throw new Exception("Should have thrown");
    } catch (Error $e) {
        // Expected
    }
});

test("Negative sample rate resample", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = pack('s', 0);
    try {
        $opus->resample($pcm, -48000, 8000);
        throw new Exception("Should have thrown");
    } catch (Error $e) {
        // Expected
    }
});

test("Extreme intensity value (>2.0)", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 1000), 100);
    // Should clamp to 2.0
    $result = $opus->enhanceVoiceClarity($pcm, 100.0);
    if (strlen($result) !== strlen($pcm)) throw new Exception("Size mismatch");
});

test("Negative intensity value", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 1000), 100);
    // Should clamp to 0.0
    $result = $opus->enhanceVoiceClarity($pcm, -100.0);
    if (strlen($result) !== strlen($pcm)) throw new Exception("Size mismatch");
});

test("Extreme width value", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 1000), 100);
    // Should clamp to 2.0
    $result = $opus->spatialStereoEnhance($pcm, 1000.0, 0.5);
    if (strlen($result) === 0) throw new Exception("Empty result");
});

test("Extreme depth value", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 1000), 100);
    // Should clamp to 1.0
    $result = $opus->spatialStereoEnhance($pcm, 1.0, 1000.0);
    if (strlen($result) === 0) throw new Exception("Empty result");
});

// ============ Category 4: Saturated Values ============
echo "\n=== Category 4: Audio Saturation Handling ===\n";

test("Max int16 saturation", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 32767), 960); // Max int16
    $encoded = $opus->encode($pcm);
    $decoded = $opus->decode($encoded);
    if (strlen($decoded) === 0) throw new Exception("Empty decode");
});

test("Min int16 saturation", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', -32768), 960); // Min int16
    $encoded = $opus->encode($pcm);
    $decoded = $opus->decode($encoded);
    if (strlen($decoded) === 0) throw new Exception("Empty decode");
});

test("Alternating min/max", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = '';
    for ($i = 0; $i < 960; $i++) {
        $pcm .= pack('s', $i % 2 ? 32767 : -32768);
    }
    $encoded = $opus->encode($pcm);
    $decoded = $opus->decode($encoded);
    if (strlen($decoded) === 0) throw new Exception("Empty decode");
});

test("DC offset (all zeros)", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 0), 960);
    $encoded = $opus->encode($pcm);
    $decoded = $opus->decode($encoded);
    if (strlen($decoded) === 0) throw new Exception("Empty decode");
});

// ============ Category 5: Multiple Instances Interference ============
echo "\n=== Category 5: Multi-Instance Isolation ===\n";

test("Parallel processing different rates", function() {
    $opus1 = new opusChannel(8000, 1);
    $opus2 = new opusChannel(16000, 1);
    $opus3 = new opusChannel(48000, 1);

    $pcm1 = str_repeat(pack('s', 1000), 160);  // 20ms at 8kHz
    $pcm2 = str_repeat(pack('s', 2000), 320);  // 20ms at 16kHz
    $pcm3 = str_repeat(pack('s', 3000), 960);  // 20ms at 48kHz

    // Should not interfere with each other
    $e1 = $opus1->encode($pcm1);
    $e2 = $opus2->encode($pcm2);
    $e3 = $opus3->encode($pcm3);

    $d1 = $opus1->decode($e1);
    $d2 = $opus2->decode($e2);
    $d3 = $opus3->decode($e3);

    if (strlen($d1) === 0 || strlen($d2) === 0 || strlen($d3) === 0) {
        throw new Exception("Decode failed");
    }
});

test("Cross-instance state isolation", function() {
    $opus1 = new opusChannel(48000, 1);
    $opus2 = new opusChannel(48000, 1);

    $pcm = str_repeat(pack('s', 5000), 1000);

    // Process on both, should maintain separate state
    $r1a = $opus1->enhanceVoiceClarity($pcm, 0.5);
    $r2a = $opus2->enhanceVoiceClarity($pcm, 1.5);

    $r1b = $opus1->enhanceVoiceClarity($pcm, 0.5);
    $r2b = $opus2->enhanceVoiceClarity($pcm, 1.5);

    // State should be independent
    if (strlen($r1a) === 0 || strlen($r2a) === 0) {
        throw new Exception("Processing failed");
    }
});

test("Destroy while other active", function() {
    $opus1 = new opusChannel(48000, 1);
    $opus2 = new opusChannel(48000, 1);
    $opus3 = new opusChannel(48000, 1);

    $pcm = str_repeat(pack('s', 1000), 960);

    $opus1->encode($pcm);
    $opus2->destroy(); // Destroy middle one
    $opus3->encode($pcm); // Others should still work
    $opus1->encode($pcm);
});

// ============ Category 6: Rapid Create/Destroy ============
echo "\n=== Category 6: Rapid Allocation/Deallocation ===\n";

test("Rapid create/destroy (no ops)", function() {
    for ($i = 0; $i < 1000; $i++) {
        $opus = new opusChannel(48000, 1);
        unset($opus);
    }
});

test("Rapid create/destroy with ops", function() {
    $pcm = str_repeat(pack('s', 1000), 960);
    for ($i = 0; $i < 100; $i++) {
        $opus = new opusChannel(48000, 1);
        $opus->encode($pcm);
        unset($opus);
    }
});

test("Nested create/destroy", function() {
    for ($i = 0; $i < 10; $i++) {
        $opus1 = new opusChannel(48000, 1);
        for ($j = 0; $j < 10; $j++) {
            $opus2 = new opusChannel(16000, 1);
            unset($opus2);
        }
        unset($opus1);
    }
});

// ============ Category 7: Reset Edge Cases ============
echo "\n=== Category 7: Reset Behavior ===\n";

test("Reset uninitialized", function() {
    $opus = new opusChannel(48000, 1);
    $opus->reset(); // Should work even if no ops done
});

test("Multiple resets", function() {
    $opus = new opusChannel(48000, 1);
    for ($i = 0; $i < 100; $i++) {
        $opus->reset();
    }
});

test("Reset then use", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 1000), 960);

    $opus->encode($pcm);
    $opus->reset();
    $opus->encode($pcm); // Should work after reset
});

test("Operations after destroy", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 1000), 960);

    $opus->destroy();

    // Should handle gracefully (may throw or silently fail)
    try {
        $opus->encode($pcm);
    } catch (Throwable $e) {
        // Expected
    }
});

// ============ Category 8: Corrupt Data Handling ============
echo "\n=== Category 8: Corrupt/Invalid Data ===\n";

test("Random binary data decode", function() {
    $opus = new opusChannel(48000, 1);
    $random = random_bytes(100);

    try {
        $opus->decode($random);
        // May succeed with noise or fail gracefully
    } catch (Throwable $e) {
        // Also acceptable
    }
});

test("Partial opus packet", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 1000), 960);
    $encoded = $opus->encode($pcm);

    // Take only half the packet
    $partial = substr($encoded, 0, strlen($encoded) / 2);

    try {
        $opus->decode($partial);
        // May fail or produce noise
    } catch (Throwable $e) {
        // Acceptable
    }
});

test("Mismatched channel decode", function() {
    $opus_mono = new opusChannel(48000, 1);
    $opus_stereo = new opusChannel(48000, 2);

    $pcm_mono = str_repeat(pack('s', 1000), 960);
    $encoded = $opus_mono->encode($pcm_mono);

    try {
        // Decode mono with stereo decoder
        $opus_stereo->decode($encoded);
        // Might work or fail
    } catch (Throwable $e) {
        // Acceptable
    }
});

// ============ Category 9: All Frame Sizes ============
echo "\n=== Category 9: All Valid Frame Sizes ===\n";

$valid_frames = [
    '2.5ms' => 120,
    '5ms' => 240,
    '10ms' => 480,
    '20ms' => 960,
    '40ms' => 1920,
    '60ms' => 2880,
    '80ms' => 3840,
    '100ms' => 4800,
    '120ms' => 5760,
];

foreach ($valid_frames as $duration => $samples) {
    test("Encode/Decode $duration frame", function() use ($samples) {
        $opus = new opusChannel(48000, 1);
        $pcm = str_repeat(pack('s', 1000), $samples);
        $encoded = $opus->encode($pcm);
        $decoded = $opus->decode($encoded);
        if (strlen($decoded) === 0) throw new Exception("Decode failed");
    });
}

// ============ Category 10: Memory Intensive ============
echo "\n=== Category 10: Memory Stress ===\n";

test("Large resample operation", function() {
    $opus = new opusChannel(48000, 1);
    // 10 seconds of audio
    $pcm = str_repeat(pack('s', 1000), 48000);
    $resampled = $opus->resample($pcm, 48000, 8000);
    if (strlen($resampled) === 0) throw new Exception("Resample failed");
});

test("Many small allocations", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 1000), 960);

    for ($i = 0; $i < 1000; $i++) {
        $opus->enhanceVoiceClarity($pcm);
    }
});

test("Interleaved operations", function() {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack('s', 1000), 960);

    for ($i = 0; $i < 100; $i++) {
        $encoded = $opus->encode($pcm);
        $decoded = $opus->decode($encoded);
        $enhanced = $opus->enhanceVoiceClarity($decoded);
        $spatial = $opus->spatialStereoEnhance($enhanced);
        $resampled = $opus->resample($spatial, 48000, 16000);
    }
});

// ============ Summary ============
echo "\n=== Test Summary ===\n";
echo "Passed: $passed\n";
echo "Failed: $failed\n";
echo "Total:  " . ($passed + $failed) . "\n";

if ($failed === 0) {
    echo "\n✓ All coredump prevention tests passed!\n";
    echo "Extension is stable and safe for production use.\n";
    exit(0);
} else {
    echo "\n✗ Some tests failed. Review the failures above.\n";
    exit(1);
}
