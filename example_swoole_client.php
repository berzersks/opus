<?php
/**
 * Swoole WebSocket Client for Opus Audio Server
 * Tests the audio streaming server
 */

use Swoole\Coroutine;
use Swoole\Coroutine\Http\Client;

if (!extension_loaded('swoole')) {
    die("✗ Swoole extension required\n");
}

if (!extension_loaded('opus')) {
    die("✗ Opus extension required\n");
}

echo "=== Opus Audio Client Test ===\n\n";

// Generate test audio (440Hz sine wave)
function generateTestAudio($samples = 960, $frequency = 440, $sample_rate = 48000) {
    $pcm = '';
    for ($i = 0; $i < $samples; $i++) {
        $value = (int)(sin(2 * M_PI * $frequency * $i / $sample_rate) * 16000);
        $pcm .= pack('s', $value);
    }
    return $pcm;
}

Coroutine\run(function() {
    $client = new Client('127.0.0.1', 9501);
    $client->set(['timeout' => 10]);

    // Upgrade to WebSocket
    $ret = $client->upgrade('/');

    if (!$ret) {
        echo "✗ Failed to connect to server\n";
        return;
    }

    echo "✓ Connected to server\n\n";

    // Create Opus encoder
    $opus = new opusChannel(48000, 1);
    echo "✓ Opus encoder created\n\n";

    // Test 1: Send ping
    echo "[Test 1] Sending ping...\n";
    $client->push(json_encode(['command' => 'ping']));

    $frame = $client->recv(2.0);
    if ($frame) {
        $msg = json_decode($frame->data, true);
        echo "  Response: {$msg['type']}\n\n";
    }

    // Test 2: Get stats
    echo "[Test 2] Getting stats...\n";
    $client->push(json_encode(['command' => 'stats']));

    $frame = $client->recv(2.0);
    if ($frame) {
        $msg = json_decode($frame->data, true);
        echo "  Client ID: {$msg['client_id']}\n";
        echo "  Sample Rate: {$msg['sample_rate']}\n";
        echo "  Channels: {$msg['channels']}\n\n";
    }

    // Test 3: Send audio data
    echo "[Test 3] Sending audio data...\n";

    $sent_count = 0;
    $received_count = 0;
    $total_processing_time = 0;

    for ($i = 0; $i < 10; $i++) {
        // Generate and encode audio
        $pcm = generateTestAudio(960, 440 + ($i * 10));
        $encoded = $opus->encode($pcm);

        // Send to server
        $client->push($encoded, WEBSOCKET_OPCODE_BINARY);
        $sent_count++;

        // Receive response (could be audio or stats)
        Coroutine::create(function() use ($client, &$received_count, &$total_processing_time) {
            $frame = $client->recv(2.0);
            if ($frame) {
                if ($frame->opcode === WEBSOCKET_OPCODE_BINARY) {
                    $received_count++;
                    echo ".";
                } else {
                    $msg = json_decode($frame->data, true);
                    if ($msg['type'] === 'stats') {
                        $total_processing_time += $msg['processing_time_ms'];
                    }
                }
            }
        });

        Coroutine::sleep(0.1); // 100ms between frames
    }

    echo "\n";
    echo "  Sent: $sent_count frames\n";
    echo "  Received: $received_count frames\n";

    if ($total_processing_time > 0) {
        echo "  Avg processing time: " . round($total_processing_time / $sent_count, 2) . "ms\n";
    }

    echo "\n";

    // Test 4: Change configuration
    echo "[Test 4] Changing configuration...\n";
    $client->push(json_encode([
        'command' => 'config',
        'sample_rate' => 16000,
        'channels' => 1
    ]));

    $frame = $client->recv(2.0);
    if ($frame) {
        $msg = json_decode($frame->data, true);
        echo "  Config updated: {$msg['sample_rate']}Hz, {$msg['channels']}ch\n\n";
    }

    // Test 5: Reset processor
    echo "[Test 5] Resetting processor...\n";
    $client->push(json_encode(['command' => 'reset']));

    $frame = $client->recv(2.0);
    if ($frame) {
        $msg = json_decode($frame->data, true);
        echo "  Reset: {$msg['type']}\n\n";
    }

    // Test 6: Stress test with concurrent sends
    echo "[Test 6] Stress test (100 frames)...\n";

    $start = microtime(true);
    $errors = 0;

    for ($i = 0; $i < 100; $i++) {
        Coroutine::create(function() use ($client, $opus, &$errors) {
            try {
                $pcm = generateTestAudio(960);
                $encoded = $opus->encode($pcm);
                $client->push($encoded, WEBSOCKET_OPCODE_BINARY);

                if ($i % 10 == 0) {
                    echo ".";
                }
            } catch (Throwable $e) {
                $errors++;
            }
        });

        if ($i % 10 == 0) {
            Coroutine::sleep(0.01);
        }
    }

    $elapsed = microtime(true) - $start;

    echo "\n";
    echo "  Time: " . round($elapsed, 2) . "s\n";
    echo "  Throughput: " . round(100 / $elapsed, 2) . " frames/sec\n";
    echo "  Errors: $errors\n\n";

    // Cleanup
    $client->close();
    $opus->destroy();

    echo "✓ All tests completed\n";
});
