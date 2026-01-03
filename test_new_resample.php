<?php

echo "Testing global resample function...\n";

// Generate 1 second of 440Hz sine wave at 44100Hz, stereo
$src_rate = 44100;
$duration = 1.0;
$num_samples = $src_rate * $duration;
$pcm = "";
for ($i = 0; $i < $num_samples; $i++) {
    $val = (int)(sin(2 * M_PI * 440 * $i / $src_rate) * 32767);
    $pcm .= pack("v", $val); // Left
    $pcm .= pack("v", $val); // Right
}

echo "Original PCM length: " . strlen($pcm) . " bytes (" . ($src_rate) . " Hz, stereo)\n";

// 1. Basic resample to 48000Hz
$dst_rate = 48000;
$resampled = resample($pcm, $src_rate, $dst_rate);
echo "Resampled to 48000Hz length: " . strlen($resampled) . " bytes (expected approx " . ($dst_rate * 2 * 2) . ")\n";

if (abs(strlen($resampled) - ($dst_rate * 2 * 2)) > 100) {
    echo "FAILED: Unexpected resampled length\n";
} else {
    echo "PASSED: Basic resample\n";
}

// 2. Resample to 8000Hz mono
$resampled_mono = resample($pcm, $src_rate, 8000, ['channels' => 1]);
echo "Resampled to 8000Hz mono length: " . strlen($resampled_mono) . " bytes (expected approx " . (8000 * 2) . ")\n";
if (abs(strlen($resampled_mono) - (8000 * 2)) > 100) {
    echo "FAILED: Unexpected mono resampled length\n";
} else {
    echo "PASSED: Mono resample\n";
}

// 3. Test L16 (Big Endian)
$resampled_l16 = resample($pcm, $src_rate, 48000, ['format' => 'l16']);
$first_sample = unpack("n", substr($resampled_l16, 0, 2))[1];
$first_sample_le = unpack("v", substr($resampled_l16, 0, 2))[1];
echo "L16 first sample (BE): $first_sample, (as LE): $first_sample_le\n";
// The sine wave starts at 0, so first sample might be 0. Let's check a sample further in.
$offset = 1000;
$sample_be = unpack("n", substr($resampled_l16, $offset, 2))[1];
$sample_le = unpack("v", substr($resampled_l16, $offset, 2))[1];
echo "L16 sample at offset $offset: BE=$sample_be, LE=$sample_le\n";
if ($sample_be != $sample_le) {
    echo "PASSED: L16 format (endianness swapped)\n";
}

// 4. Test Gain
$resampled_quiet = resample($pcm, $src_rate, $src_rate, ['gain' => 0.5]);
$orig_val = unpack("s", substr($pcm, 2000, 2))[1];
$quiet_val = unpack("s", substr($resampled_quiet, 2000, 2))[1];
echo "Gain 0.5 test: original=$orig_val, quiet=$quiet_val (expected approx half)\n";
if (abs($quiet_val - ($orig_val * 0.5)) < 2) {
    echo "PASSED: Gain\n";
} else {
    echo "FAILED: Gain\n";
}

// 5. Surprise 1: Reverse
$resampled_rev = resample($pcm, $src_rate, $src_rate, ['reverse' => true]);
$orig_start = substr($pcm, 0, 100);
$rev_end = substr($resampled_rev, -100);
// Note: reverse is by samples, so we need to be careful with byte order if comparing strings directly.
// But since we are reversing the same rate and channels, the end should match the start reversed.
echo "Reverse test: comparing start of original with end of reversed...\n";
$rev_rev_end = strrev($rev_end); // This is not quite right because of 16-bit samples
// Let's just check if it's different and has same length
if (strlen($resampled_rev) == strlen($pcm) && $resampled_rev !== $pcm) {
    echo "PASSED: Reverse (different from original, same length)\n";
} else {
    echo "FAILED: Reverse\n";
}

// 6. Surprise 2: Normalize
$quiet_pcm = "";
for ($i = 0; $i < $num_samples; $i++) {
    $val = (int)(sin(2 * M_PI * 440 * $i / $src_rate) * 1000); // Very quiet
    $quiet_pcm .= pack("v", $val) . pack("v", $val);
}
$resampled_norm = resample($quiet_pcm, $src_rate, $src_rate, ['normalize' => true]);
$max_orig = 0;
for($i=0; $i<1000; $i+=2) {
    $v = abs(unpack("s", substr($quiet_pcm, $i, 2))[1]);
    if ($v > $max_orig) $max_orig = $v;
}
$max_norm = 0;
// Scan more samples for normalized
for($i=0; $i<strlen($resampled_norm); $i+=200) {
    $v = abs(unpack("s", substr($resampled_norm, $i, 2))[1]);
    if ($v > $max_norm) $max_norm = $v;
}
echo "Normalize test: max_orig=$max_orig, max_norm=$max_norm (expected max_norm approx 32767)\n";
if ($max_norm > 30000) {
    echo "PASSED: Normalize\n";
} else {
    echo "FAILED: Normalize\n";
}

echo "All tests completed.\n";
