# Swoole + Opus Integration Tests

Complete test suite for verifying Opus extension works correctly in high-concurrency Swoole environments.

## Requirements

- PHP 8.0+
- Swoole extension (4.8+)
- Opus extension

## Installation

```bash
# Install Swoole
pecl install swoole

# Or compile from source
git clone https://github.com/swoole/swoole-src.git
cd swoole-src
phpize
./configure
make && make install
```

## Test Files

### 1. `test_swoole_parallel.php`
Parallel processing tests using Swoole coroutines.

**Tests:**
- Concurrent encoding (100 operations)
- Concurrent encode/decode pipelines (50 pipelines)
- Mixed sample rates processing
- Concurrent audio enhancement
- Concurrent resampling
- Spatial stereo processing
- Full audio pipeline stress test
- High-concurrency (500 operations)
- State isolation verification
- Throughput benchmark

**Run:**
```bash
php test_swoole_parallel.php
```

**Expected Output:**
```
=== Swoole + Opus Parallel Processing Tests ===

Test: 100 concurrent encode operations... ✓ PASS
Test: 50 concurrent encode/decode pipelines... ✓ PASS
Test: Concurrent processing with different sample rates... ✓ PASS
Test: Concurrent voice clarity enhancement... ✓ PASS
Test: Concurrent resample operations... ✓ PASS
Test: Concurrent spatial stereo enhancement... ✓ PASS
Test: 50 concurrent full audio pipelines... ✓ PASS
Test: 500 rapid concurrent operations... ✓ PASS
Test: No shared state interference between coroutines... ✓ PASS
Test: Throughput benchmark (1000 operations)... (1234.56 ops/sec) ✓ PASS

=== Swoole Test Summary ===
Passed: 10
Failed: 0
Total:  10

✓ All Swoole concurrency tests passed!
```

### 2. `example_swoole_audio_server.php`
Production-ready WebSocket audio streaming server.

**Features:**
- Real-time Opus audio streaming
- Voice enhancement processing
- Multi-client support
- Per-client isolated processing
- Low-latency coroutine-based
- WebSocket protocol
- HTTP status endpoint

**Run:**
```bash
php example_swoole_audio_server.php
```

**Endpoints:**
- WebSocket: `ws://localhost:9501`
- HTTP: `http://localhost:9501`
- Status API: `http://localhost:9501/status`

**WebSocket Protocol:**

**Text Messages (JSON Commands):**
```json
// Ping
{"command": "ping"}

// Get stats
{"command": "stats"}

// Reset processor
{"command": "reset"}

// Change configuration
{
  "command": "config",
  "sample_rate": 48000,
  "channels": 1
}
```

**Binary Messages:**
Send Opus-encoded audio data as binary WebSocket frames.

**Server Responses:**
```json
// Welcome
{
  "type": "welcome",
  "message": "Connected to Opus Audio Server",
  "client_id": 1,
  "sample_rate": 48000,
  "channels": 1
}

// Stats
{
  "type": "stats",
  "processing_time_ms": 1.23,
  "input_size": 120,
  "output_size": 80,
  "compression_ratio": 1.5
}

// Error
{
  "type": "error",
  "message": "Error description"
}
```

### 3. `example_swoole_client.php`
Test client for the WebSocket server.

**Tests:**
- Connection establishment
- Ping/pong
- Stats retrieval
- Audio streaming
- Configuration changes
- Processor reset
- Stress test (100 frames)

**Run:**
```bash
# In one terminal, start server:
php example_swoole_audio_server.php

# In another terminal, run client:
php example_swoole_client.php
```

**Expected Output:**
```
=== Opus Audio Client Test ===

✓ Connected to server
✓ Opus encoder created

[Test 1] Sending ping...
  Response: pong

[Test 2] Getting stats...
  Client ID: 1
  Sample Rate: 48000
  Channels: 1

[Test 3] Sending audio data...
..........
  Sent: 10 frames
  Received: 10 frames
  Avg processing time: 1.23ms

[Test 4] Changing configuration...
  Config updated: 16000Hz, 1ch

[Test 5] Resetting processor...
  Reset: reset_done

[Test 6] Stress test (100 frames)...
..........
  Time: 1.45s
  Throughput: 68.97 frames/sec
  Errors: 0

✓ All tests completed
```

## Architecture

### Server Design

```
┌─────────────────────────────────────────┐
│  Swoole WebSocket Server                │
│  - 4 Worker Processes                   │
│  - 2 Task Workers                       │
│  - Max 3000 Coroutines                  │
└─────────────────────────────────────────┘
            │
            ├─ Client Table (Swoole\Table)
            │  └─ Stores client metadata
            │
            └─ OpusPool (Per-Worker)
               └─ Isolated Opus instances per client
```

### Per-Client Processing Flow

```
Client Audio (Opus)
    ↓
WebSocket Binary Frame
    ↓
Coroutine Created
    ↓
Get Client's Opus Instance (OpusPool)
    ↓
Decode Opus → PCM
    ↓
Enhance Voice Clarity
    ↓
Encode PCM → Opus
    ↓
Send Back to Client
```

### State Isolation

Each client gets:
- Dedicated `opusChannel` instance
- Isolated filter states
- Independent configuration
- No shared memory between clients

## Performance Benchmarks

### Typical Results (4-core CPU)

| Test | Operations | Time | Throughput |
|------|-----------|------|------------|
| Concurrent Encode | 100 | 0.15s | 666 ops/s |
| Full Pipeline | 50 | 0.35s | 142 ops/s |
| High Concurrency | 500 | 0.80s | 625 ops/s |
| Benchmark | 1000 | 0.90s | 1111 ops/s |

### Server Load Test

```bash
# Use Apache Bench (ab) or similar tools
# Assuming you have a load test script

wrk -t4 -c100 -d30s --latency ws://localhost:9501
```

## Troubleshooting

### Issue: "Swoole extension not found"
```bash
# Check if installed
php -m | grep swoole

# Install if missing
pecl install swoole
```

### Issue: "OpusChannel not initialized"
Make sure you're creating instances properly:
```php
// Correct
$opus = new opusChannel(48000, 1);

// Wrong - will fail
$opus->encode($data); // Without new opusChannel()
```

### Issue: High memory usage
The OpusPool keeps instances alive per client. For memory efficiency:

```php
// Clean up idle clients after timeout
if (time() - $client['last_activity'] > 300) {
    OpusPool::remove($opus_id);
}
```

### Issue: Frame size errors
Ensure audio frames are valid Opus sizes:

```php
// Valid frame sizes at 48kHz
$valid_frames = [120, 240, 480, 960, 1920, 2880, 3840, 4800, 5760];

// 20ms (recommended)
$frame_size = 960; // samples at 48kHz
```

## Production Deployment

### Recommended Server Configuration

```php
$server->set([
    'worker_num' => swoole_cpu_num() * 2,
    'task_worker_num' => swoole_cpu_num(),
    'enable_coroutine' => true,
    'max_coroutine' => 10000,
    'max_connection' => 10000,
    'heartbeat_check_interval' => 60,
    'heartbeat_idle_time' => 600,
]);
```

### Monitoring

Monitor these metrics:
- Connected clients (`$clients->count()`)
- Memory usage per worker
- Message queue depth
- Processing latency
- Dropped connections

### Logging

Add comprehensive logging:
```php
// Request logging
$server->on('message', function($server, $frame) {
    $log = [
        'time' => date('Y-m-d H:i:s'),
        'client' => $frame->fd,
        'size' => strlen($frame->data),
        'type' => $frame->opcode === WEBSOCKET_OPCODE_BINARY ? 'audio' : 'command'
    ];
    file_put_contents('audio_server.log', json_encode($log) . "\n", FILE_APPEND);
});
```

### Security

1. **Rate Limiting**
```php
// Limit messages per client
$rate_limits = []; // Per-client counters

if ($rate_limits[$fd] > 100) {
    $server->close($fd);
}
```

2. **Authentication**
```php
// Token-based auth
$server->on('open', function($server, $request) {
    $token = $request->header['authorization'] ?? '';
    if (!validateToken($token)) {
        $server->close($request->fd);
    }
});
```

3. **Payload Validation**
```php
// Check payload size
if (strlen($frame->data) > 10000) {
    $server->close($frame->fd);
}
```

## Integration Examples

### With Laravel

```php
// routes/web.php
Route::get('/opus-stream', function() {
    // Launch Swoole server
    exec('php /path/to/example_swoole_audio_server.php > /dev/null 2>&1 &');
    return view('opus_stream');
});
```

### With Hyperf

```php
// config/autoload/server.php
'servers' => [
    [
        'name' => 'opus',
        'type' => Server::SERVER_WEBSOCKET,
        'host' => '0.0.0.0',
        'port' => 9501,
        'callbacks' => [
            Event::ON_MESSAGE => [OpusWebSocketController::class, 'onMessage'],
        ],
    ],
],
```

## License

These examples are provided as-is for testing and educational purposes.

## Support

For issues, create a GitHub issue or check:
- Swoole documentation: https://wiki.swoole.com
- Opus documentation: https://opus-codec.org
