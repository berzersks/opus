<?php
/**
 * Swoole WebSocket Audio Streaming Server with Opus Codec
 *
 * Features:
 * - Real-time audio streaming with Opus compression
 * - Voice enhancement processing
 * - Multi-client support with isolated processing
 * - Low-latency coroutine-based processing
 *
 * Usage:
 *   php example_swoole_audio_server.php
 *
 * Test with WebSocket client on: ws://localhost:9501
 */

use Swoole\WebSocket\Server;
use Swoole\Http\Request;
use Swoole\WebSocket\Frame;
use Swoole\Coroutine;

if (!extension_loaded('swoole')) {
    die("✗ Swoole extension required. Install: pecl install swoole\n");
}

if (!extension_loaded('opus')) {
    die("✗ Opus extension required.\n");
}

$server = new Server("0.0.0.0", 9501);

// Server configuration
$server->set([
    'worker_num' => 4,
    'task_worker_num' => 2,
    'enable_coroutine' => true,
    'max_coroutine' => 3000,
]);

// Store client-specific Opus instances
$clients = new Swoole\Table(1024);
$clients->column('opus_id', Swoole\Table::TYPE_STRING, 64);
$clients->column('sample_rate', Swoole\Table::TYPE_INT);
$clients->column('channels', Swoole\Table::TYPE_INT);
$clients->column('connected_at', Swoole\Table::TYPE_INT);
$clients->create();

// Opus instance pool (per-worker)
class OpusPool {
    private static $instances = [];

    public static function get($id, $sample_rate = 48000, $channels = 1) {
        if (!isset(self::$instances[$id])) {
            self::$instances[$id] = new opusChannel($sample_rate, $channels);
            echo "[OpusPool] Created instance: $id (rate: $sample_rate, ch: $channels)\n";
        }
        return self::$instances[$id];
    }

    public static function remove($id) {
        if (isset(self::$instances[$id])) {
            self::$instances[$id]->destroy();
            unset(self::$instances[$id]);
            echo "[OpusPool] Removed instance: $id\n";
        }
    }

    public static function reset($id) {
        if (isset(self::$instances[$id])) {
            self::$instances[$id]->reset();
            echo "[OpusPool] Reset instance: $id\n";
        }
    }
}

// WebSocket connection handler
$server->on('open', function (Server $server, Request $request) use ($clients) {
    $fd = $request->fd;

    echo "[Connect] Client #$fd connected from {$request->server['remote_addr']}\n";

    // Store client info
    $opus_id = "client_$fd";
    $clients->set($fd, [
        'opus_id' => $opus_id,
        'sample_rate' => 48000,
        'channels' => 1,
        'connected_at' => time()
    ]);

    // Send welcome message
    $server->push($fd, json_encode([
        'type' => 'welcome',
        'message' => 'Connected to Opus Audio Server',
        'client_id' => $fd,
        'sample_rate' => 48000,
        'channels' => 1
    ]));
});

// Message handler
$server->on('message', function (Server $server, Frame $frame) use ($clients) {
    $fd = $frame->fd;

    if (!$clients->exists($fd)) {
        echo "[Error] Unknown client: $fd\n";
        return;
    }

    $client = $clients->get($fd);
    $opus_id = $client['opus_id'];

    // Handle binary audio data
    if ($frame->opcode === WEBSOCKET_OPCODE_BINARY) {
        Coroutine::create(function() use ($server, $fd, $frame, $opus_id, $client) {
            try {
                $start = microtime(true);

                // Get Opus instance for this client
                $opus = OpusPool::get($opus_id, $client['sample_rate'], $client['channels']);

                // Decode Opus data
                $decoded_pcm = $opus->decode($frame->data);

                // Apply voice enhancement
                $enhanced_pcm = $opus->enhanceVoiceClarity($decoded_pcm, 1.2);

                // Re-encode with better quality
                $opus->setBitrate(64000);

                // Split into frames if needed (20ms frames = 960 samples at 48kHz)
                $frame_size = 960 * $client['channels'] * 2; // bytes
                $total_size = strlen($enhanced_pcm);

                if ($total_size >= $frame_size) {
                    // Take first frame
                    $frame_data = substr($enhanced_pcm, 0, $frame_size);
                    $encoded = $opus->encode($frame_data);

                    $elapsed = (microtime(true) - $start) * 1000;

                    // Send processed audio back
                    $server->push($fd, $encoded, WEBSOCKET_OPCODE_BINARY);

                    // Send stats
                    $server->push($fd, json_encode([
                        'type' => 'stats',
                        'processing_time_ms' => round($elapsed, 2),
                        'input_size' => strlen($frame->data),
                        'output_size' => strlen($encoded),
                        'compression_ratio' => round(strlen($frame->data) / strlen($encoded), 2)
                    ]));
                }
            } catch (Throwable $e) {
                echo "[Error] Client $fd processing failed: {$e->getMessage()}\n";
                $server->push($fd, json_encode([
                    'type' => 'error',
                    'message' => $e->getMessage()
                ]));
            }
        });
    }

    // Handle JSON commands
    else if ($frame->opcode === WEBSOCKET_OPCODE_TEXT) {
        try {
            $data = json_decode($frame->data, true);

            if (!$data || !isset($data['command'])) {
                return;
            }

            switch ($data['command']) {
                case 'config':
                    // Update client configuration
                    if (isset($data['sample_rate'])) {
                        $client['sample_rate'] = $data['sample_rate'];
                    }
                    if (isset($data['channels'])) {
                        $client['channels'] = $data['channels'];
                    }
                    $clients->set($fd, $client);

                    // Recreate Opus instance
                    OpusPool::remove($opus_id);
                    OpusPool::get($opus_id, $client['sample_rate'], $client['channels']);

                    $server->push($fd, json_encode([
                        'type' => 'config_updated',
                        'sample_rate' => $client['sample_rate'],
                        'channels' => $client['channels']
                    ]));
                    break;

                case 'reset':
                    // Reset audio processor state
                    OpusPool::reset($opus_id);
                    $server->push($fd, json_encode([
                        'type' => 'reset_done'
                    ]));
                    break;

                case 'ping':
                    $server->push($fd, json_encode([
                        'type' => 'pong',
                        'timestamp' => time()
                    ]));
                    break;

                case 'stats':
                    $uptime = time() - $client['connected_at'];
                    $server->push($fd, json_encode([
                        'type' => 'stats',
                        'client_id' => $fd,
                        'uptime_seconds' => $uptime,
                        'sample_rate' => $client['sample_rate'],
                        'channels' => $client['channels']
                    ]));
                    break;
            }
        } catch (Throwable $e) {
            echo "[Error] Command processing failed: {$e->getMessage()}\n";
        }
    }
});

// Disconnect handler
$server->on('close', function (Server $server, $fd) use ($clients) {
    if ($clients->exists($fd)) {
        $client = $clients->get($fd);
        $opus_id = $client['opus_id'];

        // Cleanup Opus instance
        OpusPool::remove($opus_id);

        // Remove client
        $clients->del($fd);

        echo "[Disconnect] Client #$fd disconnected\n";
    }
});

// HTTP handler (for status page)
$server->on('request', function (Request $request, Swoole\Http\Response $response) use ($clients) {
    if ($request->server['request_uri'] === '/status') {
        $stats = [
            'server' => 'Swoole Opus Audio Server',
            'swoole_version' => swoole_version(),
            'opus_loaded' => extension_loaded('opus'),
            'clients_connected' => $clients->count(),
            'uptime' => time() - $GLOBALS['server_start_time'],
        ];

        $response->header('Content-Type', 'application/json');
        $response->end(json_encode($stats, JSON_PRETTY_PRINT));
    } else {
        $response->header('Content-Type', 'text/html');
        $response->end(<<<HTML
<!DOCTYPE html>
<html>
<head>
    <title>Opus Audio Server</title>
    <style>
        body { font-family: Arial; max-width: 800px; margin: 50px auto; padding: 20px; }
        .status { background: #e8f5e9; padding: 20px; border-radius: 8px; }
        .info { margin: 10px 0; }
        code { background: #f5f5f5; padding: 2px 6px; border-radius: 3px; }
    </style>
</head>
<body>
    <h1>🎵 Swoole + Opus Audio Server</h1>
    <div class="status">
        <h2>Server Status</h2>
        <div class="info"><strong>WebSocket:</strong> ws://localhost:9501</div>
        <div class="info"><strong>Status API:</strong> <a href="/status">/status</a></div>
        <div class="info"><strong>Clients:</strong> <span id="clients">0</span></div>
    </div>

    <h2>Example Usage (JavaScript)</h2>
    <pre><code>const ws = new WebSocket('ws://localhost:9501');

ws.onopen = () => {
    console.log('Connected');

    // Send Opus-encoded audio (binary)
    ws.send(opusData);

    // Or send command (JSON)
    ws.send(JSON.stringify({
        command: 'config',
        sample_rate: 48000,
        channels: 1
    }));
};

ws.onmessage = (event) => {
    if (event.data instanceof Blob) {
        // Received Opus audio data
        console.log('Received audio');
    } else {
        // Received JSON message
        const msg = JSON.parse(event.data);
        console.log(msg);
    }
};</code></pre>

    <h2>Commands</h2>
    <ul>
        <li><code>{"command": "ping"}</code> - Ping server</li>
        <li><code>{"command": "stats"}</code> - Get client stats</li>
        <li><code>{"command": "reset"}</code> - Reset audio processor</li>
        <li><code>{"command": "config", "sample_rate": 48000, "channels": 1}</code> - Change config</li>
    </ul>

    <script>
        setInterval(() => {
            fetch('/status')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('clients').textContent = data.clients_connected;
                });
        }, 1000);
    </script>
</body>
</html>
HTML
        );
    }
});

$GLOBALS['server_start_time'] = time();

echo "╔════════════════════════════════════════════════════════╗\n";
echo "║       Swoole + Opus Audio Streaming Server            ║\n";
echo "╚════════════════════════════════════════════════════════╝\n";
echo "\n";
echo "WebSocket: ws://0.0.0.0:9501\n";
echo "HTTP:      http://0.0.0.0:9501\n";
echo "\n";
echo "Server starting...\n\n";

$server->start();
