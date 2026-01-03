<?php

/**
 * Script de Simulação de Telefonia Real
 * Lê um arquivo WAV, processa frame a frame e converte frequências
 */

$wav_file = '/home/lotus/PROJETOS/opus/audio_48000_stereo.wav';

if (!file_exists($wav_file)) {
    die("Erro: Arquivo $wav_file não encontrado.\n");
}

echo "Iniciando simulação de telefonia...\n";
echo "Lendo arquivo: $wav_file\n";

$data = file_get_contents($wav_file);
// Pula cabeçalho WAV de 44 bytes para pegar o PCM bruto (assumindo 16-bit 48k stereo)
$pcm_raw = substr($data, 44);
$input_rate = 48000;
$input_channels = 2;

// Tamanho do frame: 20ms de áudio em 48kHz stereo (16-bit)
// 48000 * 0.02 = 960 amostras por canal
// 960 * 2 canais * 2 bytes = 3840 bytes por frame
$frame_size = 3840; 

echo "Dividindo áudio em frames de " . ($frame_size / 4) . " amostras (20ms)...\n";
$frames = str_split($pcm_raw, $frame_size);

$scenarios = [
    ['rate' => 8000,  'channels' => 1, 'name' => 'Narrowband Mono (G.711 style)'],
    ['rate' => 16000, 'channels' => 1, 'name' => 'Wideband Mono (G.722 style)'],
    ['rate' => 44100, 'channels' => 2, 'name' => 'CD Quality Stereo'],
    ['rate' => 32000, 'channels' => 2, 'name' => 'Ultra-wideband Stereo']
];

foreach ($scenarios as $scenario) {
    echo "\n--- Testando cenário: {$scenario['name']} ({$scenario['rate']}Hz, " . ($scenario['channels'] == 1 ? 'Mono' : 'Stereo') . ") ---\n";
    
    $processed_pcm = "";
    $start_time = microtime(true);
    
    foreach ($frames as $index => $frame) {
        // Ignora último frame se estiver incompleto
        if (strlen($frame) < $frame_size) continue;
        
        try {
            $converted = resample($frame, $input_rate, $scenario['rate'], [
                'input_channels' => $input_channels,
                'output_channels' => $scenario['channels'],
                'normalize' => ($index === 0) // Normaliza apenas o primeiro frame como teste de funcionalidade
            ]);
            
            $processed_pcm .= $converted;
            
            if ($index % 50 === 0) {
                echo "Processado frame $index...\n";
            }
        } catch (Error $e) {
            echo "Erro no frame $index: " . $e->getMessage() . "\n";
            break;
        }
    }
    
    $end_time = microtime(true);
    $duration = $end_time - $start_time;
    
    echo "Concluído em " . round($duration, 4) . "s\n";
    echo "Tamanho final: " . strlen($processed_pcm) . " bytes\n";
    
    // Verificação básica de integridade
    $expected_ratio = $scenario['rate'] / $input_rate;
    $expected_channels_ratio = $scenario['channels'] / $input_channels;
    $expected_size = strlen($pcm_raw) * $expected_ratio * $expected_channels_ratio;
    
    $diff = abs(strlen($processed_pcm) - $expected_size);
    $percent_diff = ($diff / $expected_size) * 100;
    
    if ($percent_diff < 5) {
        echo "RESULTADO: SUCESSO (Diferença de tamanho: " . round($percent_diff, 2) . "%)\n";
    } else {
        echo "RESULTADO: ALERTA (Diferença de tamanho significativa: " . round($percent_diff, 2) . "%)\n";
    }
}

echo "\nSimulação finalizada.\n";
