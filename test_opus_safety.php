<?php
/**
 * Comprehensive safety test for Opus extension
 * Tests for memory leaks, segfaults, and proper resource cleanup
 */

echo "=== Opus Extension Safety Tests ===\n\n";

// Test 1: Basic initialization and cleanup
echo "Test 1: Basic initialization and cleanup... ";
try {
    $opus = new opusChannel(48000, 1);
    $opus->destroy();
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 2: Multiple instances (check for static variable issues)
echo "Test 2: Multiple instances... ";
try {
    $opus1 = new opusChannel(48000, 1);
    $opus2 = new opusChannel(16000, 2);
    $opus3 = new opusChannel(24000, 1);

    // All should work independently
    $opus1->setBitrate(64000);
    $opus2->setBitrate(32000);
    $opus3->setBitrate(48000);

    $opus1->destroy();
    $opus2->destroy();
    $opus3->destroy();
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 3: Invalid parameters validation
echo "Test 3: Invalid parameters validation... ";
$errors = [];
try {
    new opusChannel(44100, 1); // Invalid sample rate
    $errors[] = "Invalid sample rate not caught";
} catch (Throwable $e) {
    // Expected
}

try {
    new opusChannel(48000, 3); // Invalid channels
    $errors[] = "Invalid channels not caught";
} catch (Throwable $e) {
    // Expected
}

try {
    $opus = new opusChannel(48000, 1);
    $opus->setBitrate(1000000); // Invalid bitrate
    $errors[] = "Invalid bitrate not caught";
} catch (Throwable $e) {
    // Expected
}

if (empty($errors)) {
    echo "✓ PASS\n";
} else {
    echo "✗ FAIL: " . implode(", ", $errors) . "\n";
}

// Test 4: Encode/Decode cycle
echo "Test 4: Encode/Decode cycle... ";
try {
    $opus = new opusChannel(48000, 1);

    // Generate test audio (20ms frame = 960 samples at 48kHz)
    $samples = 960; // 20ms at 48kHz
    $pcm = '';
    for ($i = 0; $i < $samples; $i++) {
        $value = (int)(sin(2 * M_PI * 440 * $i / 48000) * 16000);
        $pcm .= pack('s', $value);
    }

    // Encode
    $encoded = $opus->encode($pcm);
    if (empty($encoded)) {
        throw new Exception("Encode returned empty data");
    }

    // Decode
    $decoded = $opus->decode($encoded);
    if (empty($decoded)) {
        throw new Exception("Decode returned empty data");
    }

    // Test multiple frames
    for ($i = 0; $i < 10; $i++) {
        $encoded = $opus->encode($pcm);
        $decoded = $opus->decode($encoded);
    }

    $opus->destroy();
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 5: Resample functionality (per-instance state)
echo "Test 5: Resample with multiple instances... ";
try {
    $opus1 = new opusChannel(48000, 1);
    $opus2 = new opusChannel(48000, 1);

    // Generate test audio
    $pcm = '';
    for ($i = 0; $i < 4800; $i++) {
        $pcm .= pack('s', rand(-16000, 16000));
    }

    // Both should resample independently
    $resampled1 = $opus1->resample($pcm, 48000, 8000);
    $resampled2 = $opus2->resample($pcm, 48000, 16000);

    if (strlen($resampled1) === 0 || strlen($resampled2) === 0) {
        throw new Exception("Resample returned empty data");
    }

    // Different rates should produce different sizes
    if (strlen($resampled1) === strlen($resampled2)) {
        throw new Exception("Resample not working correctly");
    }

    $opus1->destroy();
    $opus2->destroy();
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 6: enhanceVoiceClarity (per-instance state)
echo "Test 6: enhanceVoiceClarity with multiple instances... ";
try {
    $opus1 = new opusChannel(48000, 1);
    $opus2 = new opusChannel(48000, 1);

    $pcm = '';
    for ($i = 0; $i < 4800; $i++) {
        $value = (int)(sin(2 * M_PI * 440 * $i / 48000) * 16000);
        $pcm .= pack('s', $value);
    }

    // Both should process independently
    $enhanced1 = $opus1->enhanceVoiceClarity($pcm, 0.5);
    $enhanced2 = $opus2->enhanceVoiceClarity($pcm, 1.5);

    if (strlen($enhanced1) !== strlen($pcm) || strlen($enhanced2) !== strlen($pcm)) {
        throw new Exception("enhanceVoiceClarity changed output size");
    }

    $opus1->destroy();
    $opus2->destroy();
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 7: spatialStereoEnhance (per-instance state)
echo "Test 7: spatialStereoEnhance with multiple instances... ";
try {
    $opus1 = new opusChannel(48000, 1);
    $opus2 = new opusChannel(48000, 2);

    $pcm_mono = '';
    for ($i = 0; $i < 4800; $i++) {
        $pcm_mono .= pack('s', rand(-16000, 16000));
    }

    $pcm_stereo = '';
    for ($i = 0; $i < 4800; $i++) {
        $pcm_stereo .= pack('s', rand(-16000, 16000));
        $pcm_stereo .= pack('s', rand(-16000, 16000));
    }

    // Both should process independently
    $spatial1 = $opus1->spatialStereoEnhance($pcm_mono, 1.0, 0.5);
    $spatial2 = $opus2->spatialStereoEnhance($pcm_stereo, 1.5, 0.8);

    // Output should always be stereo
    $expected_size = 4800 * 2 * 2; // samples * channels * bytes_per_sample
    if (strlen($spatial1) !== $expected_size || strlen($spatial2) !== $expected_size) {
        throw new Exception("spatialStereoEnhance output size mismatch");
    }

    $opus1->destroy();
    $opus2->destroy();
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 8: Reset functionality
echo "Test 8: Reset functionality... ";
try {
    $opus = new opusChannel(48000, 1);

    $pcm = '';
    for ($i = 0; $i < 4800; $i++) {
        $pcm .= pack('s', rand(-16000, 16000));
    }

    // Process some data
    $opus->enhanceVoiceClarity($pcm);
    $opus->resample($pcm, 48000, 8000);

    // Reset should clear all state
    $opus->reset();

    // Should still work after reset
    $opus->enhanceVoiceClarity($pcm);

    $opus->destroy();
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 9: Empty data handling
echo "Test 9: Empty data handling... ";
try {
    $opus = new opusChannel(48000, 1);

    // Empty data should not crash
    $result1 = $opus->resample('', 48000, 8000);
    $result2 = $opus->enhanceVoiceClarity('');
    $result3 = $opus->spatialStereoEnhance('');

    if (strlen($result1) !== 0 || strlen($result2) !== 0 || strlen($result3) !== 0) {
        throw new Exception("Empty input should return empty output");
    }

    $opus->destroy();
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 10: Automatic cleanup (no explicit destroy)
echo "Test 10: Automatic cleanup (destructor)... ";
try {
    for ($i = 0; $i < 10; $i++) {
        $opus = new opusChannel(48000, 1);
        // Not calling destroy() - should be cleaned up automatically
    }
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 11: Stress test - many operations
echo "Test 11: Stress test (100 operations)... ";
try {
    $opus = new opusChannel(48000, 1);

    // 100ms frame = 4800 samples at 48kHz (valid Opus frame)
    $pcm = '';
    for ($i = 0; $i < 4800; $i++) {
        $value = (int)(sin(2 * M_PI * 440 * $i / 48000) * 16000);
        $pcm .= pack('s', $value);
    }

    for ($i = 0; $i < 100; $i++) {
        $encoded = $opus->encode($pcm);
        $decoded = $opus->decode($encoded);
        $enhanced = $opus->enhanceVoiceClarity($decoded);
        $resampled = $opus->resample($enhanced, 48000, 8000);
    }

    $opus->destroy();
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 12: Double destroy safety
echo "Test 12: Double destroy safety... ";
try {
    $opus = new opusChannel(48000, 1);
    $opus->destroy();
    $opus->destroy(); // Should not crash
    echo "✓ PASS\n";
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

// Test 13: Invalid data size handling
echo "Test 13: Invalid data size handling... ";
$errors = [];
try {
    $opus = new opusChannel(48000, 1);

    // Odd number of bytes (invalid for int16)
    try {
        $opus->encode("abc");
        $errors[] = "Invalid encode data not caught";
    } catch (Throwable $e) {
        // Expected
    }

    try {
        $opus->resample("abc", 48000, 8000);
        $errors[] = "Invalid resample data not caught";
    } catch (Throwable $e) {
        // Expected
    }

    $opus->destroy();

    if (empty($errors)) {
        echo "✓ PASS\n";
    } else {
        echo "✗ FAIL: " . implode(", ", $errors) . "\n";
    }
} catch (Throwable $e) {
    echo "✗ FAIL: " . $e->getMessage() . "\n";
}

echo "\n=== All tests completed ===\n";
echo "Check for memory leaks with: valgrind --leak-check=full php test_opus_safety.php\n";
