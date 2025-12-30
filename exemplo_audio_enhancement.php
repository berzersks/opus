<?php
/**
 * ========================================
 * EXEMPLOS DE USO DOS NOVOS MÉTODOS
 * ========================================
 *
 * Demonstração dos métodos revolucionários de processamento de áudio:
 * 1. enhanceVoiceClarity() - Clarificador Inteligente de Voz
 * 2. spatialStereoEnhance() - Expansor Espacial de Estéreo
 */

// Carrega um arquivo de áudio PCM (16-bit, 48kHz)
$pcmData = file_get_contents('audio_entrada.raw');

// ====================================================================
// MÉTODO 1: enhanceVoiceClarity() - Clarificador de Voz
// ====================================================================
// Remove ruídos, realça frequências vocais e normaliza o áudio
// Perfeito para: podcasts, chamadas VoIP, gravações com ruído de fundo

$opus = new opusChannel(48000, 1); // 48kHz, Mono

// Intensity: 0.0 a 2.0 (quanto maior, mais agressivo o processamento)
// - 0.5 = suave, preserva mais do áudio original
// - 1.0 = balanceado (padrão)
// - 1.5 = intenso, remove muito ruído e realça voz
$audioClaro = $opus->enhanceVoiceClarity($pcmData, 1.2);

// O QUE ELE FAZ:
// ✓ Filtro passa-banda otimizado para voz (300Hz - 3400Hz)
// ✓ Gate de ruído adaptativo (remove silêncio e ruído de fundo)
// ✓ Compressor dinâmico (equilibra volume alto e baixo)
// ✓ Saturação suave (previne clipping, som mais "quente")
// ✓ Ganho inteligente (aumenta volume geral sem distorção)

file_put_contents('audio_voz_clara.raw', $audioClaro);
echo "✅ Voz clarificada! Ruído reduzido, clareza aumentada.\n";

// ====================================================================
// MÉTODO 2: spatialStereoEnhance() - Expansor Espacial
// ====================================================================
// Cria efeito 3D espacial, transforma mono em estéreo ou expande estéreo
// Perfeito para: música, áudio ambiente, experiência imersiva

$opusStereo = new opusChannel(48000, 1); // Funciona com mono ou stereo!

// Width: 0.0 a 2.0 (largura do campo estéreo)
// - 0.0 = mono total
// - 1.0 = estéreo normal (padrão)
// - 1.5 = estéreo expandido (mais separação L/R)
// - 2.0 = estéreo ultra-wide (máxima separação)

// Depth: 0.0 a 1.0 (profundidade espacial/reverb)
// - 0.0 = sem profundidade
// - 0.5 = profundidade moderada (padrão)
// - 1.0 = máxima profundidade espacial

$audio3D = $opusStereo->spatialStereoEnhance($pcmData, 1.6, 0.7);

// O QUE ELE FAZ:
// ✓ Mid-Side Processing (técnica profissional de masterização)
// ✓ All-Pass Filter (cria diferença de fase entre L/R)
// ✓ Haas Effect (delay diferencial para profundidade)
// ✓ Pseudo-reverb sutil (adiciona "ar" e presença)
// ✓ Converte mono → estéreo automaticamente
// ✓ Limitador suave (previne distorção)

file_put_contents('audio_3d_espacial.raw', $audio3D);
echo "✅ Áudio espacializado! Som envolvente e imersivo.\n";

// ====================================================================
// PIPELINE COMPLETO: Voz Clara + Espacialização
// ====================================================================
// Combina os dois métodos para resultado PROFISSIONAL!

$opusPro = new opusChannel(48000, 1);

// Passo 1: Clarifica a voz, remove ruídos
$audioLimpo = $opusPro->enhanceVoiceClarity($pcmData, 1.0);

// Passo 2: Adiciona espacialidade e profundidade
$audioFinal = $opusPro->spatialStereoEnhance($audioLimpo, 1.3, 0.5);

file_put_contents('audio_profissional.raw', $audioFinal);
echo "✅ Pipeline completo! Áudio de qualidade profissional.\n";

// ====================================================================
// CASOS DE USO PRÁTICOS
// ====================================================================

echo "\n🎤 CASOS DE USO:\n\n";

echo "1. PODCAST/NARRAÇÃO:\n";
echo "   \$audio = \$opus->enhanceVoiceClarity(\$pcm, 1.3);\n";
echo "   → Remove ruído de ventilador, teclado, ambiente\n";
echo "   → Voz mais presente e inteligível\n\n";

echo "2. MÚSICA/AMBIENTE:\n";
echo "   \$audio = \$opus->spatialStereoEnhance(\$pcm, 1.8, 0.8);\n";
echo "   → Transforma mono em estéreo rico\n";
echo "   → Som mais envolvente e cinematográfico\n\n";

echo "3. VOIP/CHAMADA:\n";
echo "   \$audio = \$opus->enhanceVoiceClarity(\$pcm, 1.5);\n";
echo "   → Remove ecos e reverb indesejado\n";
echo "   → Comprime dinamicamente para clareza\n\n";

echo "4. MASTERIZAÇÃO ÁUDIO:\n";
echo "   \$limpo = \$opus->enhanceVoiceClarity(\$pcm, 0.8);\n";
echo "   \$final = \$opus->spatialStereoEnhance(\$limpo, 1.4, 0.6);\n";
echo "   → Pipeline completo de produção\n\n";

// ====================================================================
// INTEGRAÇÃO COM ENCODE/DECODE
// ====================================================================

echo "💡 DICA PRO: Combine com encode/decode para compressão!\n\n";

$opusCodec = new opusChannel(48000, 1);

// Processa áudio
$audioProcessado = $opusCodec->enhanceVoiceClarity($pcmData, 1.2);
$audio3D = $opusCodec->spatialStereoEnhance($audioProcessado, 1.5, 0.6);

// Codifica em Opus (altamente comprimido)
$opusCodec->setBitrate(64000); // 64kbps = excelente qualidade
$encoded = $opusCodec->encode($audio3D);

echo "Tamanho original: " . strlen($pcmData) . " bytes\n";
echo "Tamanho codificado: " . strlen($encoded) . " bytes\n";
echo "Compressão: " . round((1 - strlen($encoded)/strlen($pcmData)) * 100, 1) . "%\n\n";

// Decodifica
$decoded = $opusCodec->decode($encoded);
file_put_contents('audio_final_opus.raw', $decoded);

echo "✅ Pipeline completo com compressão Opus!\n";

// ====================================================================
// COMPARAÇÃO DE PARÂMETROS
// ====================================================================

echo "\n📊 GUIA DE PARÂMETROS:\n\n";

echo "enhanceVoiceClarity(intensity):\n";
echo "├─ 0.5  → Suave (preserva características originais)\n";
echo "├─ 1.0  → Balanceado (recomendado para uso geral)\n";
echo "├─ 1.5  → Intenso (máxima remoção de ruído)\n";
echo "└─ 2.0  → Agressivo (pode soar artificial)\n\n";

echo "spatialStereoEnhance(width, depth):\n";
echo "├─ width:\n";
echo "│  ├─ 0.5 → Estéreo sutil\n";
echo "│  ├─ 1.0 → Estéreo normal\n";
echo "│  ├─ 1.5 → Estéreo expandido\n";
echo "│  └─ 2.0 → Ultra-wide\n";
echo "└─ depth:\n";
echo "   ├─ 0.3 → Profundidade leve\n";
echo "   ├─ 0.5 → Profundidade moderada\n";
echo "   ├─ 0.7 → Profundidade acentuada\n";
echo "   └─ 1.0 → Máxima profundidade (quase reverb)\n";

echo "\n🎉 PRONTO! Experimente os parâmetros e ouça a diferença!\n";

?>
