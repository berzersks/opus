<?php
/**
 * Teste de conversão Mono <-> Stereo com medição de energia de volume
 * Garante que não há perda de energia na conversão
 *
 * Uso:
 *   php test_channel_conversion.php                    (usa sinais sintéticos)
 *   php test_channel_conversion.php arquivo.pcm        (usa arquivo PCM)
 *   php test_channel_conversion.php arquivo.pcm mono   (especifica tipo)
 *   php test_channel_conversion.php arquivo.pcm stereo (especifica tipo)
 */

// Função para calcular a energia RMS (Root Mean Square) do sinal
function calculateRMS(string $pcm_data): float {
    if (strlen($pcm_data) < 2) {
        return 0.0;
    }

    $samples = unpack('s*', $pcm_data); // Descompacta como signed int16
    $sum_squares = 0.0;
    $count = count($samples);

    foreach ($samples as $sample) {
        $normalized = $sample / 32768.0;
        $sum_squares += $normalized * $normalized;
    }

    return sqrt($sum_squares / $count);
}

// Função para gerar sinal de teste (onda senoidal)
function generateTestSignal(int $sample_rate, float $frequency, float $duration, int $channels = 1): string {
    $num_samples = (int)($sample_rate * $duration);
    $pcm = '';

    for ($i = 0; $i < $num_samples; $i++) {
        $t = $i / $sample_rate;
        $value = (int)(sin(2 * M_PI * $frequency * $t) * 16384); // Amplitude de 50%

        // Para mono: 1 sample; Para stereo: 2 samples (L, R)
        for ($ch = 0; $ch < $channels; $ch++) {
            $pcm .= pack('s', $value);
        }
    }

    return $pcm;
}

// Função para detectar tipo de arquivo (mono/stereo) por heurística
function detectChannelType(string $pcm_data, int $sample_rate = 48000): string {
    if (strlen($pcm_data) < 8) {
        return 'unknown';
    }

    // Tenta como estéreo: verifica se há diferença entre canais L/R
    $samples = unpack('s*', $pcm_data);
    $num_frames = min(1000, (int)(count($samples) / 2));

    $correlation = 0.0;
    for ($i = 0; $i < $num_frames; $i++) {
        $left = $samples[$i * 2 + 1] ?? 0;
        $right = $samples[$i * 2 + 2] ?? 0;
        $correlation += abs($left - $right);
    }

    $avg_diff = $correlation / $num_frames;

    // Se diferença média for muito baixa, provavelmente é mono duplicado ou arquivo mono
    if ($avg_diff < 100) {
        return 'mono';
    }

    return 'stereo';
}

// Função para ler arquivo WAV e extrair PCM + metadados
function readWavFile(string $wav_path): array {
    if (!file_exists($wav_path)) {
        throw new Exception("Arquivo WAV não encontrado: $wav_path");
    }

    $data = file_get_contents($wav_path);
    if (strlen($data) < 44) {
        throw new Exception("Arquivo WAV inválido (muito pequeno)");
    }

    // Verifica header RIFF
    $riff = substr($data, 0, 4);
    if ($riff !== 'RIFF') {
        throw new Exception("Não é um arquivo RIFF");
    }

    // Verifica formato WAVE
    $wave = substr($data, 8, 4);
    if ($wave !== 'WAVE') {
        throw new Exception("Não é um arquivo WAVE");
    }

    // Procura chunk fmt
    $offset = 12;
    $fmt_found = false;
    $audio_format = 0;
    $channels = 0;
    $sample_rate = 0;
    $bits_per_sample = 0;

    while ($offset < strlen($data) - 8) {
        $chunk_id = substr($data, $offset, 4);
        $chunk_size = unpack('V', substr($data, $offset + 4, 4))[1];

        if ($chunk_id === 'fmt ') {
            $fmt_data = substr($data, $offset + 8, $chunk_size);
            $audio_format = unpack('v', substr($fmt_data, 0, 2))[1];
            $channels = unpack('v', substr($fmt_data, 2, 2))[1];
            $sample_rate = unpack('V', substr($fmt_data, 4, 4))[1];
            $bits_per_sample = unpack('v', substr($fmt_data, 14, 2))[1];
            $fmt_found = true;
        } elseif ($chunk_id === 'data') {
            if (!$fmt_found) {
                throw new Exception("Chunk 'data' encontrado antes de 'fmt'");
            }
            $pcm_data = substr($data, $offset + 8, $chunk_size);

            return [
                'pcm_data' => $pcm_data,
                'sample_rate' => $sample_rate,
                'channels' => $channels,
                'bits_per_sample' => $bits_per_sample,
                'audio_format' => $audio_format,
                'duration' => $chunk_size / ($sample_rate * $channels * ($bits_per_sample / 8))
            ];
        }

        $offset += 8 + $chunk_size;
    }

    throw new Exception("Chunk 'data' não encontrado no arquivo WAV");
}

// Função para criar arquivo WAV a partir de PCM
function createWavFile(string $pcm_data, int $sample_rate, int $channels, string $output_path): void {
    $data_size = strlen($pcm_data);
    $byte_rate = $sample_rate * $channels * 2; // 16-bit = 2 bytes
    $block_align = $channels * 2;

    // RIFF header
    $wav = 'RIFF';
    $wav .= pack('V', 36 + $data_size); // ChunkSize
    $wav .= 'WAVE';

    // fmt subchunk
    $wav .= 'fmt ';
    $wav .= pack('V', 16); // Subchunk1Size (16 for PCM)
    $wav .= pack('v', 1);  // AudioFormat (1 = PCM)
    $wav .= pack('v', $channels); // NumChannels
    $wav .= pack('V', $sample_rate); // SampleRate
    $wav .= pack('V', $byte_rate); // ByteRate
    $wav .= pack('v', $block_align); // BlockAlign
    $wav .= pack('v', 16); // BitsPerSample

    // data subchunk
    $wav .= 'data';
    $wav .= pack('V', $data_size); // Subchunk2Size
    $wav .= $pcm_data;

    file_put_contents($output_path, $wav);
}

echo "=== Teste de Conversão Mono <-> Stereo ===\n\n";

// ===== CONFIGURAÇÃO DO ARQUIVO DE ENTRADA =====
// Descomente a linha abaixo e especifique o caminho do arquivo WAV
$input_wav_file = '/home/lotus/PROJETOS/opus/48000.wav';
//$input_wav_file = null;

// Verifica argumentos da linha de comando (sobrescreve variável acima)
if (isset($argv[1])) {
    $input_wav_file = $argv[1];
}

if ($input_wav_file) {
    echo "Modo: Teste com arquivo WAV\n";
    echo "Arquivo: $input_wav_file\n\n";

    try {
        $wav_info = readWavFile($input_wav_file);
    } catch (Exception $e) {
        die("ERRO ao ler arquivo WAV: " . $e->getMessage() . "\n");
    }

    $pcm_data = $wav_info['pcm_data'];
    $original_sample_rate = $wav_info['sample_rate'];
    $original_channels = $wav_info['channels'];
    $bits_per_sample = $wav_info['bits_per_sample'];

    echo "Informações do arquivo WAV:\n";
    echo "  Sample Rate: {$original_sample_rate} Hz\n";
    echo "  Canais: {$original_channels}\n";
    echo "  Bits/Sample: {$bits_per_sample}\n";
    echo "  Duração: " . number_format($wav_info['duration'], 2) . " segundos\n";
    echo "  Tamanho PCM: " . strlen($pcm_data) . " bytes\n";

    if ($bits_per_sample != 16) {
        die("ERRO: Apenas 16-bit PCM é suportado\n");
    }

    if ($wav_info['audio_format'] != 1) {
        die("ERRO: Apenas PCM não comprimido é suportado (audio_format=1)\n");
    }

    $channel_type = ($original_channels == 1) ? 'mono' : 'stereo';
    echo "  Tipo: $channel_type\n\n";

    // Aplica resample se necessário
    $target_sample_rate = 48000;
    $needs_resample = ($original_sample_rate != $target_sample_rate);

    if ($needs_resample) {
        echo "Aplicando resample de {$original_sample_rate}Hz para {$target_sample_rate}Hz...\n";
        $opus_temp = new opusChannel($target_sample_rate, $original_channels);
        $pcm_data = $opus_temp->resample($pcm_data, $original_sample_rate, $target_sample_rate);
        $opus_temp->destroy();
        echo "Resample concluído: " . strlen($pcm_data) . " bytes\n\n";
    }

    $use_file_mode = true;
    $base_filename = pathinfo($input_wav_file, PATHINFO_FILENAME);
} else {
    echo "Modo: Teste com sinais sintéticos\n";
    echo "Dica: Defina \$input_wav_file no código ou use:\n";
    echo "      php test_channel_conversion.php arquivo.wav\n\n";
    $use_file_mode = false;
    $target_sample_rate = 48000;
}

// Cria instância OpusChannel
$opus = new opusChannel($target_sample_rate, 1);

// Mostra informações da extensão
$info = $opus->getInfo();
echo "Informações da extensão:\n";
echo "  Versão: {$info['extension_version']}\n";
echo "  libopus: {$info['libopus_version']}\n";
echo "  libsoxr: " . ($opus->hasLibsoxr() ? "Disponível" : "Fallback linear") . "\n";
echo "\n";

$tolerance = 0.01; // Tolerância de 1%

if ($use_file_mode) {
    // === TESTE COM ARQUIVO WAV ===
    echo "=== TESTE COM ARQUIVO WAV ===\n";
    echo str_repeat("=", 50) . "\n\n";

    if ($channel_type === 'mono') {
        // Arquivo Mono
        echo "Teste: Mono -> Stereo -> Mono\n";
        echo str_repeat("-", 50) . "\n";

        $mono_original = $pcm_data;
        $rms_original = calculateRMS($mono_original);

        echo "Arquivo mono original:\n";
        echo "  Tamanho: " . strlen($mono_original) . " bytes\n";
        echo "  Samples: " . (strlen($mono_original) / 2) . "\n";
        echo "  RMS: " . number_format($rms_original, 6) . "\n\n";

        // Converte Mono -> Stereo
        $stereo = $opus->monoToStereo($mono_original);
        $rms_stereo = calculateRMS($stereo);

        echo "Após conversão para Stereo:\n";
        echo "  Tamanho: " . strlen($stereo) . " bytes\n";
        echo "  Samples: " . (strlen($stereo) / 4) . " frames\n";
        echo "  RMS: " . number_format($rms_stereo, 6) . "\n";
        echo "  Relação: " . number_format(($rms_stereo / $rms_original) * 100, 2) . "%\n\n";

        // Salva arquivo stereo
        $output_stereo_wav = $base_filename . '_stereo.wav';
        createWavFile($stereo, $target_sample_rate, 2, $output_stereo_wav);
        echo "Arquivo stereo salvo: $output_stereo_wav\n\n";

        // Converte Stereo -> Mono
        $mono_restored = $opus->stereoToMono($stereo);
        $rms_restored = calculateRMS($mono_restored);

        echo "Após conversão de volta para Mono:\n";
        echo "  Tamanho: " . strlen($mono_restored) . " bytes\n";
        echo "  Samples: " . (strlen($mono_restored) / 2) . "\n";
        echo "  RMS: " . number_format($rms_restored, 6) . "\n";
        echo "  Relação com original: " . number_format(($rms_restored / $rms_original) * 100, 2) . "%\n\n";

        // Salva arquivo mono restaurado
        $output_mono_wav = $base_filename . '_restored.wav';
        createWavFile($mono_restored, $target_sample_rate, 1, $output_mono_wav);
        echo "Arquivo mono restaurado salvo: $output_mono_wav\n\n";

        // Verifica energia
        $diff_stereo = abs($rms_stereo - $rms_original) / $rms_original;
        $diff_restored = abs($rms_restored - $rms_original) / $rms_original;

        if ($diff_stereo <= $tolerance) {
            echo "✓ Mono->Stereo: Energia preservada (diferença: " . number_format($diff_stereo * 100, 2) . "%)\n";
        } else {
            echo "✗ Mono->Stereo: Perda de energia detectada (diferença: " . number_format($diff_stereo * 100, 2) . "%)\n";
        }

        if ($diff_restored <= $tolerance) {
            echo "✓ Stereo->Mono: Energia preservada (diferença: " . number_format($diff_restored * 100, 2) . "%)\n";
        } else {
            echo "✗ Stereo->Mono: Perda de energia detectada (diferença: " . number_format($diff_restored * 100, 2) . "%)\n";
        }

    } else {
        // Arquivo Stereo
        echo "Teste: Stereo -> Mono -> Stereo\n";
        echo str_repeat("-", 50) . "\n";

        $stereo_original = $pcm_data;
        $rms_original = calculateRMS($stereo_original);

        echo "Arquivo stereo original:\n";
        echo "  Tamanho: " . strlen($stereo_original) . " bytes\n";
        echo "  Frames: " . (strlen($stereo_original) / 4) . "\n";
        echo "  RMS: " . number_format($rms_original, 6) . "\n\n";

        // Converte Stereo -> Mono
        $mono = $opus->stereoToMono($stereo_original);
        $rms_mono = calculateRMS($mono);

        echo "Após conversão para Mono:\n";
        echo "  Tamanho: " . strlen($mono) . " bytes\n";
        echo "  Samples: " . (strlen($mono) / 2) . "\n";
        echo "  RMS: " . number_format($rms_mono, 6) . "\n";
        echo "  Relação: " . number_format(($rms_mono / $rms_original) * 100, 2) . "%\n\n";

        // Salva arquivo mono
        $output_mono_wav = $base_filename . '_mono.wav';
        createWavFile($mono, $target_sample_rate, 1, $output_mono_wav);
        echo "Arquivo mono salvo: $output_mono_wav\n\n";

        // Converte Mono -> Stereo
        $stereo_restored = $opus->monoToStereo($mono);
        $rms_restored = calculateRMS($stereo_restored);

        echo "Após conversão de volta para Stereo:\n";
        echo "  Tamanho: " . strlen($stereo_restored) . " bytes\n";
        echo "  Frames: " . (strlen($stereo_restored) / 4) . "\n";
        echo "  RMS: " . number_format($rms_restored, 6) . "\n";
        echo "  Relação com original: " . number_format(($rms_restored / $rms_original) * 100, 2) . "%\n\n";

        // Salva arquivo stereo restaurado
        $output_stereo_wav = $base_filename . '_restored.wav';
        createWavFile($stereo_restored, $target_sample_rate, 2, $output_stereo_wav);
        echo "Arquivo stereo restaurado salvo: $output_stereo_wav\n\n";

        // Verifica energia
        $diff_mono = abs($rms_mono - $rms_original) / $rms_original;
        $diff_restored = abs($rms_restored - $rms_original) / $rms_original;

        if ($diff_mono <= $tolerance) {
            echo "✓ Stereo->Mono: Energia preservada (diferença: " . number_format($diff_mono * 100, 2) . "%)\n";
        } else {
            echo "✗ Stereo->Mono: Perda de energia detectada (diferença: " . number_format($diff_mono * 100, 2) . "%)\n";
        }

        if ($diff_restored <= $tolerance) {
            echo "✓ Mono->Stereo: Energia preservada (diferença: " . number_format($diff_restored * 100, 2) . "%)\n";
        } else {
            echo "✗ Mono->Stereo: Perda de energia detectada (diferença: " . number_format($diff_restored * 100, 2) . "%)\n";
        }
    }

} else {
    // === TESTES COM SINAIS SINTÉTICOS ===

    // === TESTE 1: Mono -> Stereo -> Mono ===
    echo "TESTE 1: Mono -> Stereo -> Mono\n";
    echo str_repeat("-", 50) . "\n";

    // Gera sinal mono de teste (440Hz, 0.1s)
    $mono_original = generateTestSignal(48000, 440.0, 0.1, 1);
    $rms_original = calculateRMS($mono_original);

    echo "Sinal mono original:\n";
    echo "  Tamanho: " . strlen($mono_original) . " bytes\n";
    echo "  RMS: " . number_format($rms_original, 6) . "\n\n";

    // Converte Mono -> Stereo
    $stereo = $opus->monoToStereo($mono_original);
    $rms_stereo = calculateRMS($stereo);

    echo "Após conversão para Stereo:\n";
    echo "  Tamanho: " . strlen($stereo) . " bytes\n";
    echo "  RMS: " . number_format($rms_stereo, 6) . "\n";
    echo "  Relação: " . number_format(($rms_stereo / $rms_original) * 100, 2) . "%\n\n";

    // Converte Stereo -> Mono
    $mono_restored = $opus->stereoToMono($stereo);
    $rms_restored = calculateRMS($mono_restored);

    echo "Após conversão de volta para Mono:\n";
    echo "  Tamanho: " . strlen($mono_restored) . " bytes\n";
    echo "  RMS: " . number_format($rms_restored, 6) . "\n";
    echo "  Relação com original: " . number_format(($rms_restored / $rms_original) * 100, 2) . "%\n\n";

    // Verifica se a energia foi preservada
    $diff_stereo = abs($rms_stereo - $rms_original) / $rms_original;
    $diff_restored = abs($rms_restored - $rms_original) / $rms_original;

    if ($diff_stereo <= $tolerance) {
        echo "✓ Mono->Stereo: Energia preservada (diferença: " . number_format($diff_stereo * 100, 2) . "%)\n";
    } else {
        echo "✗ Mono->Stereo: Perda de energia detectada (diferença: " . number_format($diff_stereo * 100, 2) . "%)\n";
    }

    if ($diff_restored <= $tolerance) {
        echo "✓ Stereo->Mono: Energia preservada (diferença: " . number_format($diff_restored * 100, 2) . "%)\n";
    } else {
        echo "✗ Stereo->Mono: Perda de energia detectada (diferença: " . number_format($diff_restored * 100, 2) . "%)\n";
    }

    echo "\n";

    // === TESTE 2: Stereo -> Mono ===
    echo "TESTE 2: Sinal Stereo -> Mono\n";
    echo str_repeat("-", 50) . "\n";

    // Gera sinal estéreo de teste (440Hz, 0.1s)
    $stereo_original = generateTestSignal(48000, 440.0, 0.1, 2);
    $rms_stereo_orig = calculateRMS($stereo_original);

    echo "Sinal stereo original:\n";
    echo "  Tamanho: " . strlen($stereo_original) . " bytes\n";
    echo "  RMS: " . number_format($rms_stereo_orig, 6) . "\n\n";

    // Converte Stereo -> Mono
    $mono_from_stereo = $opus->stereoToMono($stereo_original);
    $rms_mono_from_stereo = calculateRMS($mono_from_stereo);

    echo "Após conversão para Mono:\n";
    echo "  Tamanho: " . strlen($mono_from_stereo) . " bytes\n";
    echo "  RMS: " . number_format($rms_mono_from_stereo, 6) . "\n";
    echo "  Relação: " . number_format(($rms_mono_from_stereo / $rms_stereo_orig) * 100, 2) . "%\n\n";

    $diff_mono = abs($rms_mono_from_stereo - $rms_stereo_orig) / $rms_stereo_orig;

    if ($diff_mono <= $tolerance) {
        echo "✓ Stereo->Mono: Energia preservada (diferença: " . number_format($diff_mono * 100, 2) . "%)\n";
    } else {
        echo "✗ Stereo->Mono: Perda de energia detectada (diferença: " . number_format($diff_mono * 100, 2) . "%)\n";
    }

    echo "\n";

    // === TESTE 3: Teste com diferentes frequências ===
    echo "TESTE 3: Múltiplas frequências\n";
    echo str_repeat("-", 50) . "\n";

    $frequencies = [100, 440, 1000, 4000, 8000];
    $all_passed = true;

    foreach ($frequencies as $freq) {
        $signal = generateTestSignal(48000, $freq, 0.05, 1);
        $rms_in = calculateRMS($signal);

        $stereo_sig = $opus->monoToStereo($signal);
        $mono_sig = $opus->stereoToMono($stereo_sig);
        $rms_out = calculateRMS($mono_sig);

        $diff = abs($rms_out - $rms_in) / $rms_in;
        $status = ($diff <= $tolerance) ? "✓" : "✗";

        if ($diff > $tolerance) {
            $all_passed = false;
        }

        echo sprintf("%s %5dHz: RMS In=%.6f, Out=%.6f, Diff=%.2f%%\n",
                     $status, $freq, $rms_in, $rms_out, $diff * 100);
    }

    echo "\n";

    if ($all_passed) {
        echo "✓ Todos os testes de frequência passaram!\n";
    } else {
        echo "✗ Alguns testes de frequência falharam!\n";
    }
}

echo "\n=== Testes concluídos ===\n";

$opus->destroy();
