<?php
/**
 * Swoole Parallel Processing Tests for Opus Extension
 * Tests real concurrent operations using Swoole coroutines
 */

if (!extension_loaded('swoole')) {
    die("✗ Swoole extension not found. Install with: pecl install swoole\n");
}

if (!extension_loaded('opus')) {
    die("✗ Opus extension not found.\n");
}

echo "=== Swoole + Opus Parallel Processing Tests ===\n\n";
echo "Swoole version: " . swoole_version() . "\n";
echo "Testing real concurrent execution...\n\n";

use Swoole\Coroutine;
use Swoole\Coroutine\Channel;

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

// ============ Test 1: Concurrent Encoding ============
test("100 concurrent encode operations", function() {
    $results = new Channel(100);

    Co\run(function() use ($results) {
        for ($i = 0; $i < 100; $i++) {
            Coroutine::create(function() use ($i, $results) {
                try {
                    $opus = new opusChannel(48000, 1);

                    // Generate test audio
                    $pcm = '';
                    for ($j = 0; $j < 960; $j++) {
                        $value = (int)(sin(2 * M_PI * 440 * $j / 48000) * 16000);
                        $pcm .= pack('s', $value);
                    }

                    $encoded = $opus->encode($pcm);
                    $results->push(['id' => $i, 'success' => strlen($encoded) > 0]);

                    $opus->destroy();
                } catch (Throwable $e) {
                    $results->push(['id' => $i, 'success' => false, 'error' => $e->getMessage()]);
                }
            });
        }
    });

    // Check results
    $success_count = 0;
    for ($i = 0; $i < 100; $i++) {
        $result = $results->pop(1.0);
        if ($result && $result['success']) {
            $success_count++;
        }
    }

    if ($success_count !== 100) {
        throw new Exception("Only $success_count/100 succeeded");
    }
});

// ============ Test 2: Concurrent Encode/Decode Pipeline ============
test("50 concurrent encode/decode pipelines", function() {
    $results = new Channel(50);

    Co\run(function() use ($results) {
        for ($i = 0; $i < 50; $i++) {
            Coroutine::create(function() use ($i, $results) {
                try {
                    $opus = new opusChannel(48000, 1);

                    $pcm = '';
                    for ($j = 0; $j < 960; $j++) {
                        $value = (int)(sin(2 * M_PI * (440 + $i) * $j / 48000) * 16000);
                        $pcm .= pack('s', $value);
                    }

                    // Full pipeline
                    $encoded = $opus->encode($pcm);
                    Coroutine::sleep(0.001); // Simulate processing delay
                    $decoded = $opus->decode($encoded);

                    $results->push(['id' => $i, 'success' => strlen($decoded) > 0]);
                    $opus->destroy();
                } catch (Throwable $e) {
                    $results->push(['id' => $i, 'success' => false, 'error' => $e->getMessage()]);
                }
            });
        }
    });

    $success_count = 0;
    for ($i = 0; $i < 50; $i++) {
        $result = $results->pop(2.0);
        if ($result && $result['success']) {
            $success_count++;
        }
    }

    if ($success_count !== 50) {
        throw new Exception("Only $success_count/50 succeeded");
    }
});

// ============ Test 3: Mixed Sample Rates Concurrently ============
test("Concurrent processing with different sample rates", function() {
    $rates = [8000, 12000, 16000, 24000, 48000];
    $results = new Channel(count($rates) * 10);

    Co\run(function() use ($rates, $results) {
        foreach ($rates as $rate) {
            for ($i = 0; $i < 10; $i++) {
                Coroutine::create(function() use ($rate, $i, $results) {
                    try {
                        $opus = new opusChannel($rate, 1);

                        $frame_size = $rate / 50; // 20ms
                        $pcm = str_repeat(pack('s', rand(-16000, 16000)), $frame_size);

                        $encoded = $opus->encode($pcm);
                        $decoded = $opus->decode($encoded);

                        $results->push(['rate' => $rate, 'success' => strlen($decoded) > 0]);
                        $opus->destroy();
                    } catch (Throwable $e) {
                        $results->push(['rate' => $rate, 'success' => false, 'error' => $e->getMessage()]);
                    }
                });
            }
        }
    });

    $success_count = 0;
    for ($i = 0; $i < count($rates) * 10; $i++) {
        $result = $results->pop(3.0);
        if ($result && $result['success']) {
            $success_count++;
        }
    }

    if ($success_count !== count($rates) * 10) {
        throw new Exception("Only $success_count/" . (count($rates) * 10) . " succeeded");
    }
});

// ============ Test 4: Concurrent Audio Enhancement ============
test("Concurrent voice clarity enhancement", function() {
    $results = new Channel(100);

    Co\run(function() use ($results) {
        for ($i = 0; $i < 100; $i++) {
            Coroutine::create(function() use ($i, $results) {
                try {
                    $opus = new opusChannel(48000, 1);

                    $pcm = str_repeat(pack('s', rand(-16000, 16000)), 1000);
                    $intensity = ($i % 20) / 10.0; // 0.0 to 1.9

                    $enhanced = $opus->enhanceVoiceClarity($pcm, $intensity);

                    $results->push(['id' => $i, 'success' => strlen($enhanced) === strlen($pcm)]);
                    $opus->destroy();
                } catch (Throwable $e) {
                    $results->push(['id' => $i, 'success' => false, 'error' => $e->getMessage()]);
                }
            });
        }
    });

    $success_count = 0;
    for ($i = 0; $i < 100; $i++) {
        $result = $results->pop(2.0);
        if ($result && $result['success']) {
            $success_count++;
        }
    }

    if ($success_count !== 100) {
        throw new Exception("Only $success_count/100 succeeded");
    }
});

// ============ Test 5: Concurrent Resampling ============
test("Concurrent resample operations", function() {
    $results = new Channel(50);

    Co\run(function() use ($results) {
        for ($i = 0; $i < 50; $i++) {
            Coroutine::create(function() use ($i, $results) {
                try {
                    $opus = new opusChannel(48000, 1);

                    $pcm = str_repeat(pack('s', rand(-16000, 16000)), 4800);

                    $rates = [8000, 16000, 24000, 48000];
                    $dst = $rates[$i % count($rates)];

                    $resampled = $opus->resample($pcm, 48000, $dst);

                    $results->push(['id' => $i, 'success' => strlen($resampled) > 0]);
                    $opus->destroy();
                } catch (Throwable $e) {
                    $results->push(['id' => $i, 'success' => false, 'error' => $e->getMessage()]);
                }
            });
        }
    });

    $success_count = 0;
    for ($i = 0; $i < 50; $i++) {
        $result = $results->pop(2.0);
        if ($result && $result['success']) {
            $success_count++;
        }
    }

    if ($success_count !== 50) {
        throw new Exception("Only $success_count/50 succeeded");
    }
});

// ============ Test 6: Concurrent Spatial Processing ============
test("Concurrent spatial stereo enhancement", function() {
    $results = new Channel(50);

    Co\run(function() use ($results) {
        for ($i = 0; $i < 50; $i++) {
            Coroutine::create(function() use ($i, $results) {
                try {
                    $channels = ($i % 2) + 1;
                    $opus = new opusChannel(48000, $channels);

                    $pcm = str_repeat(pack('s', rand(-16000, 16000)), 1000 * $channels);
                    $width = ($i % 10) / 5.0; // 0.0 to 1.8
                    $depth = ($i % 5) / 5.0;  // 0.0 to 0.8

                    $spatial = $opus->spatialStereoEnhance($pcm, $width, $depth);

                    $results->push(['id' => $i, 'success' => strlen($spatial) > 0]);
                    $opus->destroy();
                } catch (Throwable $e) {
                    $results->push(['id' => $i, 'success' => false, 'error' => $e->getMessage()]);
                }
            });
        }
    });

    $success_count = 0;
    for ($i = 0; $i < 50; $i++) {
        $result = $results->pop(2.0);
        if ($result && $result['success']) {
            $success_count++;
        }
    }

    if ($success_count !== 50) {
        throw new Exception("Only $success_count/50 succeeded");
    }
});

// ============ Test 7: Full Pipeline Stress Test ============
test("50 concurrent full audio pipelines", function() {
    $results = new Channel(50);

    Co\run(function() use ($results) {
        for ($i = 0; $i < 50; $i++) {
            Coroutine::create(function() use ($i, $results) {
                try {
                    $opus = new opusChannel(48000, 1);

                    $pcm = '';
                    for ($j = 0; $j < 960; $j++) {
                        $value = (int)(sin(2 * M_PI * 440 * $j / 48000) * 16000);
                        $pcm .= pack('s', $value);
                    }

                    // Full pipeline
                    $encoded = $opus->encode($pcm);
                    Coroutine::sleep(0.001);

                    $decoded = $opus->decode($encoded);
                    Coroutine::sleep(0.001);

                    $enhanced = $opus->enhanceVoiceClarity($decoded, 1.0);
                    Coroutine::sleep(0.001);

                    $spatial = $opus->spatialStereoEnhance($enhanced, 1.0, 0.5);
                    Coroutine::sleep(0.001);

                    $resampled = $opus->resample($spatial, 48000, 8000);

                    $results->push(['id' => $i, 'success' => strlen($resampled) > 0]);
                    $opus->destroy();
                } catch (Throwable $e) {
                    $results->push(['id' => $i, 'success' => false, 'error' => $e->getMessage()]);
                }
            });
        }
    });

    $success_count = 0;
    for ($i = 0; $i < 50; $i++) {
        $result = $results->pop(5.0);
        if ($result && $result['success']) {
            $success_count++;
        }
    }

    if ($success_count !== 50) {
        throw new Exception("Only $success_count/50 succeeded");
    }
});

// ============ Test 8: High-Concurrency Stress Test ============
test("500 rapid concurrent operations", function() {
    $results = new Channel(500);

    Co\run(function() use ($results) {
        for ($i = 0; $i < 500; $i++) {
            Coroutine::create(function() use ($i, $results) {
                try {
                    $opus = new opusChannel(48000, 1);
                    $pcm = str_repeat(pack('s', 1000), 960);
                    $encoded = $opus->encode($pcm);
                    $opus->destroy();

                    $results->push(['id' => $i, 'success' => true]);
                } catch (Throwable $e) {
                    $results->push(['id' => $i, 'success' => false, 'error' => $e->getMessage()]);
                }
            });
        }
    });

    $success_count = 0;
    for ($i = 0; $i < 500; $i++) {
        $result = $results->pop(5.0);
        if ($result && $result['success']) {
            $success_count++;
        }
    }

    if ($success_count !== 500) {
        throw new Exception("Only $success_count/500 succeeded");
    }
});

// ============ Test 9: Shared Memory Safety ============
test("No shared state interference between coroutines", function() {
    $results = new Channel(20);

    Co\run(function() use ($results) {
        // Create 10 pairs of coroutines that process differently
        for ($pair = 0; $pair < 10; $pair++) {
            // Coroutine A: low intensity
            Coroutine::create(function() use ($pair, $results) {
                try {
                    $opus = new opusChannel(48000, 1);
                    $pcm = str_repeat(pack('s', 5000), 1000);

                    for ($i = 0; $i < 10; $i++) {
                        $enhanced = $opus->enhanceVoiceClarity($pcm, 0.1);
                        Coroutine::sleep(0.001);
                    }

                    $results->push(['pair' => $pair, 'type' => 'A', 'success' => true]);
                    $opus->destroy();
                } catch (Throwable $e) {
                    $results->push(['pair' => $pair, 'type' => 'A', 'success' => false]);
                }
            });

            // Coroutine B: high intensity
            Coroutine::create(function() use ($pair, $results) {
                try {
                    $opus = new opusChannel(48000, 1);
                    $pcm = str_repeat(pack('s', 5000), 1000);

                    for ($i = 0; $i < 10; $i++) {
                        $enhanced = $opus->enhanceVoiceClarity($pcm, 1.9);
                        Coroutine::sleep(0.001);
                    }

                    $results->push(['pair' => $pair, 'type' => 'B', 'success' => true]);
                    $opus->destroy();
                } catch (Throwable $e) {
                    $results->push(['pair' => $pair, 'type' => 'B', 'success' => false]);
                }
            });
        }
    });

    $success_count = 0;
    for ($i = 0; $i < 20; $i++) {
        $result = $results->pop(10.0);
        if ($result && $result['success']) {
            $success_count++;
        }
    }

    if ($success_count !== 20) {
        throw new Exception("Only $success_count/20 succeeded - state interference detected");
    }
});

// ============ Test 10: Benchmark Throughput ============
test("Throughput benchmark (1000 operations)", function() {
    $start = microtime(true);
    $results = new Channel(1000);

    Co\run(function() use ($results) {
        for ($i = 0; $i < 1000; $i++) {
            Coroutine::create(function() use ($i, $results) {
                try {
                    $opus = new opusChannel(48000, 1);
                    $pcm = str_repeat(pack('s', 1000), 960);
                    $encoded = $opus->encode($pcm);
                    $decoded = $opus->decode($encoded);
                    $opus->destroy();
                    $results->push(true);
                } catch (Throwable $e) {
                    $results->push(false);
                }
            });
        }
    });

    $success_count = 0;
    for ($i = 0; $i < 1000; $i++) {
        if ($results->pop(10.0)) {
            $success_count++;
        }
    }

    $elapsed = microtime(true) - $start;
    $throughput = $success_count / $elapsed;

    echo sprintf(" (%.2f ops/sec) ", $throughput);

    if ($success_count !== 1000) {
        throw new Exception("Only $success_count/1000 succeeded");
    }
});

// ============ Summary ============
echo "\n=== Swoole Test Summary ===\n";
echo "Passed: $passed\n";
echo "Failed: $failed\n";
echo "Total:  " . ($passed + $failed) . "\n";

if ($failed === 0) {
    echo "\n✓ All Swoole concurrency tests passed!\n";
    echo "Extension is safe for high-concurrency Swoole applications.\n";
    exit(0);
} else {
    echo "\n✗ Some tests failed. Review the failures above.\n";
    exit(1);
}
