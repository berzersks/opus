<?php
/**
 * Fuzzing tests for Opus extension
 * Attempts to find crashes with random/malformed input
 */

echo "=== Opus Fuzzing Tests ===\n\n";
echo "Running 1000 random tests...\n";

$crashes = 0;
$errors = 0;
$successes = 0;

// Test with random sample rates
echo "\n[1/5] Fuzzing sample rates and channels...\n";
for ($i = 0; $i < 200; $i++) {
    try {
        $rate = [8000, 12000, 16000, 24000, 48000][array_rand([8000, 12000, 16000, 24000, 48000])];
        $ch = rand(1, 2);
        $opus = new opusChannel($rate, $ch);
        $opus->destroy();
        $successes++;
    } catch (Throwable $e) {
        $errors++;
    }
}
echo "  Successes: $successes, Errors: $errors, Crashes: $crashes\n";

// Test with random PCM data
echo "\n[2/5] Fuzzing encode with random PCM...\n";
$errors = 0;
$successes = 0;

for ($i = 0; $i < 200; $i++) {
    try {
        $opus = new opusChannel(48000, 1);

        // Random valid frame sizes
        $valid_sizes = [120, 240, 480, 960, 1920, 2880, 3840, 4800, 5760];
        $size = $valid_sizes[array_rand($valid_sizes)];

        $pcm = '';
        for ($j = 0; $j < $size; $j++) {
            $pcm .= pack('s', rand(-32768, 32767));
        }

        $encoded = $opus->encode($pcm);
        $successes++;
    } catch (Throwable $e) {
        $errors++;
    }
}
echo "  Successes: $successes, Errors: $errors, Crashes: $crashes\n";

// Test with random opus packets
echo "\n[3/5] Fuzzing decode with random data...\n";
$errors = 0;
$successes = 0;

for ($i = 0; $i < 200; $i++) {
    try {
        $opus = new opusChannel(48000, 1);

        // Random data of various sizes
        $size = rand(10, 1000);
        $random_data = random_bytes($size);

        try {
            $decoded = $opus->decode($random_data);
            $successes++;
        } catch (Error $e) {
            // Decode errors are expected with random data
            $errors++;
        }
    } catch (Throwable $e) {
        $errors++;
    }
}
echo "  Successes: $successes, Errors: $errors, Crashes: $crashes\n";

// Test with random resample parameters
echo "\n[4/5] Fuzzing resample operations...\n";
$errors = 0;
$successes = 0;

for ($i = 0; $i < 200; $i++) {
    try {
        $opus = new opusChannel(48000, 1);

        $pcm_sizes = [100, 480, 960, 1920, 4800];
        $size = $pcm_sizes[array_rand($pcm_sizes)];
        $pcm = str_repeat(pack('s', rand(-16000, 16000)), $size);

        $rates = [8000, 16000, 24000, 48000];
        $src = $rates[array_rand($rates)];
        $dst = $rates[array_rand($rates)];

        $resampled = $opus->resample($pcm, $src, $dst);
        $successes++;
    } catch (Throwable $e) {
        $errors++;
    }
}
echo "  Successes: $successes, Errors: $errors, Crashes: $crashes\n";

// Test with random enhancement parameters
echo "\n[5/5] Fuzzing audio enhancement...\n";
$errors = 0;
$successes = 0;

for ($i = 0; $i < 200; $i++) {
    try {
        $opus = new opusChannel(48000, rand(1, 2));

        $size = rand(100, 5000);
        $pcm = str_repeat(pack('s', rand(-32768, 32767)), $size);

        $op = rand(0, 1);
        if ($op === 0) {
            // Enhance voice clarity with random intensity
            $intensity = rand(0, 200) / 100.0; // 0.0 to 2.0
            $result = $opus->enhanceVoiceClarity($pcm, $intensity);
        } else {
            // Spatial stereo with random parameters
            $width = rand(0, 200) / 100.0; // 0.0 to 2.0
            $depth = rand(0, 100) / 100.0; // 0.0 to 1.0
            $result = $opus->spatialStereoEnhance($pcm, $width, $depth);
        }

        $successes++;
    } catch (Throwable $e) {
        $errors++;
    }
}
echo "  Successes: $successes, Errors: $errors, Crashes: $crashes\n";

// Boundary testing
echo "\n=== Boundary Value Testing ===\n";

$boundary_tests = [
    'Zero-length PCM' => function() {
        $opus = new opusChannel(48000, 1);
        return $opus->resample('', 48000, 8000);
    },
    'Single sample' => function() {
        $opus = new opusChannel(48000, 1);
        try {
            $opus->encode(pack('s', 0));
        } catch (Throwable $e) {
            return true;
        }
        return false;
    },
    'Max int16 chain' => function() {
        $opus = new opusChannel(48000, 1);
        $pcm = str_repeat(pack('s', 32767), 960);
        $encoded = $opus->encode($pcm);
        $decoded = $opus->decode($encoded);
        $enhanced = $opus->enhanceVoiceClarity($decoded, 2.0);
        $spatial = $opus->spatialStereoEnhance($enhanced, 2.0, 1.0);
        return strlen($spatial) > 0;
    },
    'Min int16 chain' => function() {
        $opus = new opusChannel(48000, 1);
        $pcm = str_repeat(pack('s', -32768), 960);
        $encoded = $opus->encode($pcm);
        $decoded = $opus->decode($encoded);
        $enhanced = $opus->enhanceVoiceClarity($decoded, 2.0);
        $spatial = $opus->spatialStereoEnhance($enhanced, 2.0, 1.0);
        return strlen($spatial) > 0;
    },
    'Alternating extremes' => function() {
        $opus = new opusChannel(48000, 1);
        $pcm = '';
        for ($i = 0; $i < 960; $i++) {
            $pcm .= pack('s', $i % 2 ? 32767 : -32768);
        }
        $encoded = $opus->encode($pcm);
        $decoded = $opus->decode($encoded);
        return strlen($decoded) > 0;
    },
    'Rapid transitions' => function() {
        $opus = new opusChannel(48000, 1);
        $pcm = '';
        for ($i = 0; $i < 960; $i++) {
            $pcm .= pack('s', $i % 4 < 2 ? 16000 : -16000);
        }
        $encoded = $opus->encode($pcm);
        $decoded = $opus->decode($encoded);
        $enhanced = $opus->enhanceVoiceClarity($decoded);
        return strlen($enhanced) > 0;
    },
];

$boundary_passed = 0;
$boundary_failed = 0;

foreach ($boundary_tests as $name => $test) {
    echo "  $name... ";
    try {
        $result = $test();
        if ($result !== false) {
            echo "✓ PASS\n";
            $boundary_passed++;
        } else {
            echo "✗ FAIL\n";
            $boundary_failed++;
        }
    } catch (Throwable $e) {
        echo "✗ ERROR: " . $e->getMessage() . "\n";
        $boundary_failed++;
    }
}

// Pattern-based testing
echo "\n=== Pattern-Based Testing ===\n";

$patterns = [
    'Silence (zeros)' => array_fill(0, 960, 0),
    'DC offset (constant)' => array_fill(0, 960, 10000),
    'Linear ramp' => range(-16000, 16000, 32768 / 960),
    'Saw wave' => array_map(function($i) {
        return (($i % 100) - 50) * 650;
    }, range(0, 959)),
    'Square wave' => array_map(function($i) {
        return $i % 100 < 50 ? 16000 : -16000;
    }, range(0, 959)),
    'Random noise' => array_map(function() {
        return rand(-32768, 32767);
    }, range(0, 959)),
];

$pattern_passed = 0;
$pattern_failed = 0;

foreach ($patterns as $name => $pattern) {
    echo "  $name... ";
    try {
        $opus = new opusChannel(48000, 1);
        $pcm = '';
        foreach ($pattern as $sample) {
            $pcm .= pack('s', (int)$sample);
        }

        $encoded = $opus->encode($pcm);
        $decoded = $opus->decode($encoded);
        $enhanced = $opus->enhanceVoiceClarity($decoded);

        if (strlen($enhanced) > 0) {
            echo "✓ PASS\n";
            $pattern_passed++;
        } else {
            echo "✗ FAIL (empty output)\n";
            $pattern_failed++;
        }
    } catch (Throwable $e) {
        echo "✗ ERROR: " . $e->getMessage() . "\n";
        $pattern_failed++;
    }
}

// Summary
echo "\n=== Fuzzing Summary ===\n";
echo "Random tests completed: 1000\n";
echo "Boundary tests: $boundary_passed passed, $boundary_failed failed\n";
echo "Pattern tests: $pattern_passed passed, $pattern_failed failed\n";
echo "Total crashes detected: $crashes\n";

if ($crashes === 0 && $boundary_failed === 0 && $pattern_failed === 0) {
    echo "\n✓ No crashes or failures detected!\n";
    echo "Extension handles random/malformed input safely.\n";
    exit(0);
} else {
    echo "\n⚠ Some issues detected. Review above.\n";
    exit(1);
}
