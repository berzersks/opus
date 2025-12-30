#!/bin/bash

set -e

echo "=== Opus Extension Build and Test Script ==="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check for required tools
echo "Checking required tools..."
for tool in php php-config phpize gcc make; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}✗ $tool not found${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ $tool found${NC}"
done

# Check for opus library
echo ""
echo "Checking for libopus..."
if pkg-config --exists opus; then
    echo -e "${GREEN}✓ libopus found: $(pkg-config --modversion opus)${NC}"
else
    echo -e "${RED}✗ libopus not found${NC}"
    echo "Install with: sudo apt-get install libopus-dev"
    exit 1
fi

# Clean previous build
echo ""
echo "Cleaning previous build..."
make clean 2>/dev/null || true
phpize --clean 2>/dev/null || true
rm -f modules/*.so

# Build extension
echo ""
echo "Building extension..."
phpize
./configure --enable-opus
make clean
make

if [ ! -f "modules/opus.so" ]; then
    echo -e "${RED}✗ Build failed - opus.so not found${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Build successful${NC}"

# Check for memory analysis tools
echo ""
echo "Checking for memory analysis tools..."
HAS_VALGRIND=0
if command -v valgrind &> /dev/null; then
    echo -e "${GREEN}✓ valgrind found${NC}"
    HAS_VALGRIND=1
else
    echo -e "${YELLOW}⚠ valgrind not found (optional)${NC}"
fi

# Run basic test
echo ""
echo "=== Running Basic Tests ==="
php -dextension=modules/opus.so test_opus_safety.php

# Check exit code
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Basic tests passed${NC}"
else
    echo -e "${RED}✗ Basic tests failed${NC}"
    exit 1
fi

# Run valgrind if available
if [ $HAS_VALGRIND -eq 1 ]; then
    echo ""
    echo "=== Running Valgrind Memory Check ==="
    echo "This may take a few minutes..."

    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --error-exitcode=1 \
        --errors-for-leak-kinds=all \
        --suppressions=/usr/share/php*/valgrind-suppressions 2>/dev/null \
        php -dextension=modules/opus.so test_opus_safety.php 2>&1 | tee valgrind.log

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ No memory leaks detected${NC}"
    else
        echo -e "${RED}✗ Memory issues detected - see valgrind.log${NC}"
        echo ""
        echo "Summary of issues:"
        grep -A 2 "LEAK SUMMARY" valgrind.log || true
        exit 1
    fi
fi

# Run stress test
echo ""
echo "=== Running Stress Test ==="
php -dextension=modules/opus.so -r '
for ($i = 0; $i < 1000; $i++) {
    $opus = new opusChannel(48000, 1);
    $pcm = str_repeat(pack("s", rand(-16000, 16000)), 4800);
    $encoded = $opus->encode($pcm);
    $decoded = $opus->decode($encoded);
    unset($opus);
    if ($i % 100 == 0) echo "Progress: $i/1000\n";
}
echo "Stress test completed!\n";
'

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Stress test passed${NC}"
else
    echo -e "${RED}✗ Stress test failed${NC}"
    exit 1
fi

echo ""
echo "=== All Tests Passed ==="
echo -e "${GREEN}✓ Build successful${NC}"
echo -e "${GREEN}✓ Basic tests passed${NC}"
echo -e "${GREEN}✓ Stress test passed${NC}"
if [ $HAS_VALGRIND -eq 1 ]; then
    echo -e "${GREEN}✓ No memory leaks${NC}"
fi

echo ""
echo "Extension ready to use!"
echo "Load with: php -dextension=$(pwd)/modules/opus.so"
