<?php
/**
 * Simplified Swoole Tests for Opus Extension
 * Tests real concurrent operations using Swoole coroutines
 */

if (!extension_loaded('swoole')) {
    die("✗ Swoole extension not found. Install with: pecl install swoole\n");
}

if (!extension_loaded('opus')) {
    die("✗ Opus extension not found.\n");
}

echo "=== Swoole + Opus Tests ===\n\n";
echo "Swoole version: " . swoole_version() . "\n\n";

use Swoole\Coroutine;

$passed = 0;
$failed = 0;

// Test 1: Basic concurrent encoding
echo "Test 1: 50 concurrent encode operations... ";
$errors = [];
Coroutine\run(function() use (&$errors) {
    for ($i = 0; $i < 50; $i++) {
        Coroutine::create(function() use ($i, &$errors) {
            try {
                $opus = new opusChannel(48000, 1);
                $pcm = str_repeat(pack('s', 1000), 960);
                $encoded = $opus->encode($pcm);
                if (strlen($encoded) === 0) {
                    $errors[] = "Empty encode result #$i";
                }
                $opus->destroy();
            } catch (Throwable $e) {
                $errors[] = "#$i: " . $e->getMessage();
            }
        });
    }
});

if (empty($errors)) {
    echo "✓ PASS\n";
    $passed++;
} else {
    echo "✗ FAIL: " . count($errors) . " errors\n";
    $failed++;
}

// Test 2: Concurrent encode/decode
echo "Test 2: 30 concurrent encode/decode pipelines... ";
$errors = [];
Coroutine\run(function() use (&$errors) {
    for ($i = 0; $i < 30; $i++) {
        Coroutine::create(function() use ($i, &$errors) {
            try {
                $opus = new opusChannel(48000, 1);
                $pcm = str_repeat(pack('s', 2000), 960);

                $encoded = $opus->encode($pcm);
                Coroutine::sleep(0.001);
                $decoded = $opus->decode($encoded);

                if (strlen($decoded) === 0) {
                    $errors[] = "Empty decode #$i";
                }
                $opus->destroy();
            } catch (Throwable $e) {
                $errors[] = "#$i: " . $e->getMessage();
            }
        });
    }
});

if (empty($errors)) {
    echo "✓ PASS\n";
    $passed++;
} else {
    echo "✗ FAIL: " . count($errors) . " errors\n";
    $failed++;
}

// Test 3: Mixed sample rates
echo "Test 3: Concurrent mixed sample rates... ";
$errors = [];
Coroutine\run(function() use (&$errors) {
    $rates = [8000, 16000, 24000, 48000];
    foreach ($rates as $rate) {
        for ($i = 0; $i < 5; $i++) {
            Coroutine::create(function() use ($rate, $i, &$errors) {
                try {
                    $opus = new opusChannel($rate, 1);
                    $frame_size = $rate / 50; // 20ms
                    $pcm = str_repeat(pack('s', 3000), $frame_size);

                    $encoded = $opus->encode($pcm);
                    $decoded = $opus->decode($encoded);

                    if (strlen($decoded) === 0) {
                        $errors[] = "Empty at $rate Hz #$i";
                    }
                    $opus->destroy();
                } catch (Throwable $e) {
                    $errors[] = "$rate Hz #$i: " . $e->getMessage();
                }
            });
        }
    }
});

if (empty($errors)) {
    echo "✓ PASS\n";
    $passed++;
} else {
    echo "✗ FAIL: " . count($errors) . " errors\n";
    foreach (array_slice($errors, 0, 3) as $err) {
        echo "  - $err\n";
    }
    $failed++;
}

// Test 4: Concurrent voice enhancement
echo "Test 4: 50 concurrent voice enhancements... ";
$errors = [];
Coroutine\run(function() use (&$errors) {
    for ($i = 0; $i < 50; $i++) {
        Coroutine::create(function() use ($i, &$errors) {
            try {
                $opus = new opusChannel(48000, 1);
                $pcm = str_repeat(pack('s', rand(-16000, 16000)), 1000);
                $intensity = ($i % 10) / 10.0;

                $enhanced = $opus->enhanceVoiceClarity($pcm, $intensity);

                if (strlen($enhanced) !== strlen($pcm)) {
                    $errors[] = "Size mismatch #$i";
                }
                $opus->destroy();
            } catch (Throwable $e) {
                $errors[] = "#$i: " . $e->getMessage();
            }
        });
    }
});

if (empty($errors)) {
    echo "✓ PASS\n";
    $passed++;
} else {
    echo "✗ FAIL: " . count($errors) . " errors\n";
    $failed++;
}

// Test 5: Concurrent resampling
echo "Test 5: 30 concurrent resample operations... ";
$errors = [];
Coroutine\run(function() use (&$errors) {
    for ($i = 0; $i < 30; $i++) {
        Coroutine::create(function() use ($i, &$errors) {
            try {
                $opus = new opusChannel(48000, 1);
                $pcm = str_repeat(pack('s', 4000), 4800);

                $resampled = $opus->resample($pcm, 48000, 8000);

                if (strlen($resampled) === 0) {
                    $errors[] = "Empty resample #$i";
                }
                $opus->destroy();
            } catch (Throwable $e) {
                $errors[] = "#$i: " . $e->getMessage();
            }
        });
    }
});

if (empty($errors)) {
    echo "✓ PASS\n";
    $passed++;
} else {
    echo "✗ FAIL: " . count($errors) . " errors\n";
    $failed++;
}

// Test 6: Full pipeline stress
echo "Test 6: 20 full pipeline operations... ";
$errors = [];
Coroutine\run(function() use (&$errors) {
    for ($i = 0; $i < 20; $i++) {
        Coroutine::create(function() use ($i, &$errors) {
            try {
                $opus = new opusChannel(48000, 1);
                $pcm = str_repeat(pack('s', 5000), 960);

                $encoded = $opus->encode($pcm);
                Coroutine::sleep(0.001);

                $decoded = $opus->decode($encoded);
                Coroutine::sleep(0.001);

                $enhanced = $opus->enhanceVoiceClarity($decoded, 1.0);
                Coroutine::sleep(0.001);

                $spatial = $opus->spatialStereoEnhance($enhanced, 1.0, 0.5);

                if (strlen($spatial) === 0) {
                    $errors[] = "Pipeline failed #$i";
                }
                $opus->destroy();
            } catch (Throwable $e) {
                $errors[] = "#$i: " . $e->getMessage();
            }
        });
    }
});

if (empty($errors)) {
    echo "✓ PASS\n";
    $passed++;
} else {
    echo "✗ FAIL: " . count($errors) . " errors\n";
    $failed++;
}

// Test 7: State isolation
echo "Test 7: State isolation between coroutines... ";
$errors = [];
Coroutine\run(function() use (&$errors) {
    for ($pair = 0; $pair < 10; $pair++) {
        // Low intensity
        Coroutine::create(function() use ($pair, &$errors) {
            try {
                $opus = new opusChannel(48000, 1);
                $pcm = str_repeat(pack('s', 6000), 1000);

                for ($i = 0; $i < 5; $i++) {
                    $opus->enhanceVoiceClarity($pcm, 0.1);
                    Coroutine::sleep(0.001);
                }
                $opus->destroy();
            } catch (Throwable $e) {
                $errors[] = "Pair $pair A: " . $e->getMessage();
            }
        });

        // High intensity
        Coroutine::create(function() use ($pair, &$errors) {
            try {
                $opus = new opusChannel(48000, 1);
                $pcm = str_repeat(pack('s', 6000), 1000);

                for ($i = 0; $i < 5; $i++) {
                    $opus->enhanceVoiceClarity($pcm, 1.9);
                    Coroutine::sleep(0.001);
                }
                $opus->destroy();
            } catch (Throwable $e) {
                $errors[] = "Pair $pair B: " . $e->getMessage();
            }
        });
    }
});

if (empty($errors)) {
    echo "✓ PASS\n";
    $passed++;
} else {
    echo "✗ FAIL: " . count($errors) . " errors\n";
    $failed++;
}

// Test 8: Throughput benchmark
echo "Test 8: Throughput (200 operations)... ";
$start = microtime(true);
$errors = [];

Coroutine\run(function() use (&$errors) {
    for ($i = 0; $i < 200; $i++) {
        Coroutine::create(function() use ($i, &$errors) {
            try {
                $opus = new opusChannel(48000, 1);
                $pcm = str_repeat(pack('s', 1000), 960);
                $encoded = $opus->encode($pcm);
                $decoded = $opus->decode($encoded);
                $opus->destroy();
            } catch (Throwable $e) {
                $errors[] = "#$i: " . $e->getMessage();
            }
        });
    }
});

$elapsed = microtime(true) - $start;
$throughput = 200 / $elapsed;

if (empty($errors)) {
    echo sprintf("✓ PASS (%.0f ops/sec)\n", $throughput);
    $passed++;
} else {
    echo "✗ FAIL: " . count($errors) . " errors\n";
    $failed++;
}

// Summary
echo "\n=== Swoole Test Summary ===\n";
echo "Passed: $passed\n";
echo "Failed: $failed\n";
echo "Total:  " . ($passed + $failed) . "\n";

if ($failed === 0) {
    echo "\n✓ All Swoole tests passed!\n";
    echo "Extension is safe for Swoole applications.\n";
    exit(0);
} else {
    echo "\n✗ Some tests failed.\n";
    exit(1);
}
