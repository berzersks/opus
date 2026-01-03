<?php

$wav_file = '/home/lotus/PROJETOS/opus/audio_48000_stereo.wav';
if (!file_exists($wav_file)) {
    $wav_file = '/home/lotus/PROJETOS/opus/result_48000_stereo.wav';
}

if (!file_exists($wav_file)) {
    die("Erro: Arquivo de áudio não encontrado.\n");
}

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

$data = file_get_contents($wav_file);
$pcm_raw = substr($data, 44);
$input_rate = 48000;
$input_channels = 2;

echo "Salvando resultados de testes adicionais...\n";

// 1. Audio Reverse
echo "Processando Reverse...\n";
$reversed = resample($pcm_raw, 48000, 48000, ['reverse' => true]);
file_put_contents($output_dir . '/test_reversed.wav', write_wav_header(48000, 2, strlen($reversed)) . $reversed);

// 2. Normalization
echo "Processando Normalização...\n";
$normalized = resample($pcm_raw, 48000, 48000, ['normalize' => true]);
file_put_contents($output_dir . '/test_normalized.wav', write_wav_header(48000, 2, strlen($normalized)) . $normalized);

// 3. Gain (2.0x)
echo "Processando Ganho 2.0x...\n";
$gained = resample($pcm_raw, 48000, 48000, ['gain' => 2.0]);
file_put_contents($output_dir . '/test_gain_2x.wav', write_wav_header(48000, 2, strlen($gained)) . $gained);

// 4. L16 Format (Big Endian) - Saving as .l16
echo "Processando L16 (Big Endian)...\n";
$l16 = resample($pcm_raw, 48000, 48000, ['format' => 'l16']);
file_put_contents($output_dir . '/test_format_l16.raw', $l16);

echo "Todos os testes adicionais foram salvos em $output_dir\n";
