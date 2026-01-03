<?php

/**
 * Script de Simulação de Telefonia Real
 * Lê um arquivo WAV, processa frame a frame e converte frequências
 */

$wav_file = '/home/lotus/PROJETOS/opus/audio_48000_stereo.wav';
if (!file_exists($wav_file)) {
    $wav_file = '/home/lotus/PROJETOS/opus/result_48000_stereo.wav';
}

if (!file_exists($wav_file)) {
    die("Erro: Arquivo de áudio não encontrado para teste.\n");
}

echo "Iniciando simulação de telefonia...\n";
echo "Lendo arquivo: $wav_file\n";

$data = file_get_contents($wav_file);
$pcm_raw = substr($data, 44); // Pula cabeçalho WAV
$input_rate = 48000;
$input_channels = 2;
$frame_size = 3840; // 20ms @ 48kHz Stereo

$frames = str_split($pcm_raw, $frame_size);

$scenarios = [
    ['rate' => 8000,  'channels' => 1, 'name' => 'Narrowband Mono (G.711 style)'],
    ['rate' => 16000, 'channels' => 1, 'name' => 'Wideband Mono (G.722 style)'],
    ['rate' => 44100, 'channels' => 2, 'name' => 'CD Quality Stereo'],
    ['rate' => 32000, 'channels' => 2, 'name' => 'Ultra-wideband Stereo']
];

$output_dir = __DIR__ . '/telephony_test_results';
if (!is_dir($output_dir)) {
    mkdir($output_dir, 0777, true);
}

function write_wav_header($sample_rate, $channels, $data_len) {
    $header = "RIFF";
    $header .= pack("V", 36 + $data_len);
    $header .= "WAVEfmt ";
    $header .= pack("V", 16);
    $header .= pack("v", 1); // PCM
    $header .= pack("v", $channels);
    $header .= pack("V", $sample_rate);
    $header .= pack("V", $sample_rate * $channels * 2);
    $header .= pack("v", $channels * 2);
    $header .= pack("v", 16);
    $header .= "data";
    $header .= pack("V", $data_len);
    return $header;
}

foreach ($scenarios as $scenario) {
    echo "\n--- Testando cenário: {$scenario['name']} ({$scenario['rate']}Hz) ---\n";
    $processed_pcm = "";
    
    foreach ($frames as $index => $frame) {
        if (strlen($frame) < $frame_size) continue;
        
        $converted = resample($frame, $input_rate, $scenario['rate'], [
            'input_channels' => $input_channels,
            'output_channels' => $scenario['channels'],
            'normalize' => ($index === 0)
        ]);
        
        $processed_pcm .= $converted;
    }
    
    $file_name = str_replace([' ', '(', ')', '.', '/'], '_', $scenario['name']) . ".wav";
    $file_path = $output_dir . '/' . strtolower($file_name);
    
    $wav_data = write_wav_header($scenario['rate'], $scenario['channels'], strlen($processed_pcm)) . $processed_pcm;
    file_put_contents($file_path, $wav_data);
    
    echo "Arquivo salvo: $file_path\n";
    echo "Tamanho final: " . strlen($processed_pcm) . " bytes\n";
    echo "SUCESSO.\n";
}

echo "\nSimulação finalizada.\n";
var_dump((new opusChannel(48000,1))->getInfo());