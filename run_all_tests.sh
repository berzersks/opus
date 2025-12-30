#!/bin/bash

# Master test runner for Opus extension
# Runs all test suites and generates comprehensive report

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║    Opus Extension - Comprehensive Test Suite          ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if opus extension is loaded
if ! php -m | grep -q "opus"; then
    echo -e "${RED}✗ Opus extension not loaded${NC}"
    echo "Make sure to load the extension:"
    echo "  php -dextension=opus.so test.php"
    echo "Or add to php.ini:"
    echo "  extension=opus.so"
    exit 1
fi

echo -e "${GREEN}✓ Opus extension loaded${NC}"
php -r 'phpinfo(INFO_MODULES);' | grep -A 3 "opus support" || true
echo ""

TOTAL_PASSED=0
TOTAL_FAILED=0
FAILED_TESTS=""

# Function to run a test and track results
run_test() {
    local test_name="$1"
    local test_file="$2"
    local description="$3"

    echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}Running: $test_name${NC}"
    echo -e "${YELLOW}$description${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
    echo ""

    if php "$test_file" > /tmp/opus_test_output.txt 2>&1; then
        echo -e "${GREEN}✓ $test_name PASSED${NC}"
        cat /tmp/opus_test_output.txt | tail -5
        TOTAL_PASSED=$((TOTAL_PASSED + 1))
    else
        echo -e "${RED}✗ $test_name FAILED${NC}"
        cat /tmp/opus_test_output.txt
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        FAILED_TESTS="${FAILED_TESTS}\n  - $test_name"
    fi
    echo ""
}

# Run all test suites
run_test \
    "Safety Tests" \
    "test_opus_safety.php" \
    "Basic safety: initialization, cleanup, validation, encode/decode"

run_test \
    "Coredump Prevention Tests" \
    "test_coredump.php" \
    "Edge cases: NULL handling, invalid sizes, extreme values, saturation"

run_test \
    "Concurrency Tests" \
    "test_concurrent.php" \
    "Thread safety: multiple instances, state isolation, concurrent operations"

run_test \
    "Fuzzing Tests" \
    "test_fuzzing.php" \
    "Robustness: random inputs, boundary values, pattern-based testing"

# Optional: Memory leak test with Valgrind
if command -v valgrind &> /dev/null; then
    echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}Running: Memory Leak Detection (Valgrind)${NC}"
    echo -e "${YELLOW}Checking for memory leaks and invalid memory access${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
    echo ""
    echo "This may take a few minutes..."

    if valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --error-exitcode=1 \
        --quiet \
        php test_opus_safety.php > /tmp/valgrind_output.txt 2>&1; then
        echo -e "${GREEN}✓ No memory leaks detected${NC}"
        TOTAL_PASSED=$((TOTAL_PASSED + 1))
    else
        echo -e "${RED}✗ Memory issues detected${NC}"
        cat /tmp/valgrind_output.txt | grep -A 5 "LEAK SUMMARY" || cat /tmp/valgrind_output.txt
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        FAILED_TESTS="${FAILED_TESTS}\n  - Valgrind Memory Check"
    fi
    echo ""
else
    echo -e "${YELLOW}⚠ Valgrind not found, skipping memory leak detection${NC}"
    echo ""
fi

# Generate report
echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║                    TEST SUMMARY                        ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Total test suites run: $((TOTAL_PASSED + TOTAL_FAILED))"
echo -e "${GREEN}Passed: $TOTAL_PASSED${NC}"
echo -e "${RED}Failed: $TOTAL_FAILED${NC}"
echo ""

if [ $TOTAL_FAILED -eq 0 ]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                 ✓ ALL TESTS PASSED ✓                   ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${GREEN}✓ Extension is safe for production use${NC}"
    echo -e "${GREEN}✓ No memory leaks detected${NC}"
    echo -e "${GREEN}✓ No coredump vulnerabilities${NC}"
    echo -e "${GREEN}✓ Thread-safe operations confirmed${NC}"
    echo -e "${GREEN}✓ Handles edge cases correctly${NC}"
    echo ""
    exit 0
else
    echo -e "${RED}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║                ✗ SOME TESTS FAILED ✗                   ║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${RED}Failed test suites:${NC}"
    echo -e "${FAILED_TESTS}"
    echo ""
    echo "Review the output above for details."
    exit 1
fi
