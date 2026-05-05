#!/usr/bin/env bash
set -e

# ─────────────────────────────────────────────────────────────────────────────
#  Colors & helpers
# ─────────────────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
    exit 1
}

TOTAL_TESTS=0
PASSED_TESTS=0

run_test() {
    local name="$1"
    local input="$2"
    local expected="$3"
    shift 3
    local cmd="$@"

    TOTAL_TESTS=$((TOTAL_TESTS+1))
    print_info "Running: $name"

    OUTPUT=$(printf "%b\n<END>\n" "$input" | $cmd)
    if echo "$OUTPUT" | grep -q "$expected"; then
        print_status "$name"
        PASSED_TESTS=$((PASSED_TESTS+1))
    else
        print_error "$name → expected: '$expected' got: $OUTPUT"
    fi
}

run_invalid_test() {
    local name="$1"
    shift
    TOTAL_TESTS=$((TOTAL_TESTS+1))
    print_info "Running (invalid expected): $name"

    if "$@" >/dev/null 2>&1; then
        print_error "$name → expected failure, but succeeded"
    else
        print_status "$name"
        PASSED_TESTS=$((PASSED_TESTS+1))
    fi
}

# ─────────────────────────────────────────────────────────────────────────────
#  1. Build
# ─────────────────────────────────────────────────────────────────────────────
print_info "Building project..."
./build.sh >/dev/null
print_status "Build successful"

# ─────────────────────────────────────────────────────────────────────────────
#  2. Positive tests
# ─────────────────────────────────────────────────────────────────────────────
run_test "Logger prints" "hello" "^\[logger\] hello" ./output/analyzer 5 logger
run_test "Uppercaser + logger" "hello" "^\[logger\] HELLO" ./output/analyzer 5 uppercaser logger
run_test "Flipper + logger" "abcd" "^\[logger\] dcba" ./output/analyzer 5 flipper logger
run_test "Rotator + logger" "abcd" "^\[logger\] dabc" ./output/analyzer 5 rotator logger
run_test "Expander + logger" "abc" "^\[logger\] a b c" ./output/analyzer 5 expander logger
run_test "Prefixer + logger" "hello" "^\[logger\] \[prefix\] hello" ./output/analyzer 5 prefixer logger

# ─────────────────────────────────────────────────────────────────────────────
#  3. Negative / invalid usage
# ─────────────────────────────────────────────────────────────────────────────
run_invalid_test "No arguments" ./output/analyzer
run_invalid_test "Only queue size" ./output/analyzer 5
run_invalid_test "Queue size zero" ./output/analyzer 0 logger
run_invalid_test "Queue size negative" ./output/analyzer -3 logger
run_invalid_test "Queue size non-numeric" ./output/analyzer abc logger
run_invalid_test "Invalid plugin name" ./output/analyzer 5 notaplugin

# ─────────────────────────────────────────────────────────────────────────────
#  4. Edge cases
# ─────────────────────────────────────────────────────────────────────────────
run_test "Empty string" "" "^\[logger\] $" ./output/analyzer 5 logger
run_test "Special chars preserved" "!@#$%^&*()" "^\[logger\] !@#\\$%\\^&\\*()" ./output/analyzer 5 logger
LONG=$(head -c 300 < /dev/zero | tr '\0' 'x')
run_test "Long string (300 chars)" "$LONG" "^\[logger\] ${LONG}" ./output/analyzer 50 logger
LONG2=$(head -c 1020 < /dev/zero | tr '\0' 'y')
run_test "Very long string (1020 chars)" "$LONG2" "^\[logger\] ${LONG2}" ./output/analyzer 100 logger

# ─────────────────────────────────────────────────────────────────────────────
#  5. Stress tests
# ─────────────────────────────────────────────────────────────────────────────
LINES=$(printf "line\n%.0s" {1..50})
run_test "Stress: 50 lines uppercaser→rotator→logger" "$LINES" "^\[logger\]" ./output/analyzer 10 uppercaser rotator logger
run_test "Stress: queue size 1 forces blocking" "abc" "^\[logger\] ABC" ./output/analyzer 1 uppercaser logger
run_test "Stress: mixed case + logger" "HeLLo" "^\[logger\] HELLO" ./output/analyzer 5 uppercaser logger

# ─────────────────────────────────────────────────────────────────────────────
#  6. Multiple same-plugins
# ─────────────────────────────────────────────────────────────────────────────
run_test "Double uppercaser (same as one)" "hello" "^\[logger\] HELLO" ./output/analyzer 5 uppercaser uppercaser logger
run_test "Double rotator" "abcd" "^\[logger\] cdab" ./output/analyzer 5 rotator rotator logger
run_test "Triple flipper (same as single flipper)" "hello" "^\[logger\] olleh" ./output/analyzer 5 flipper flipper flipper logger

print_info "Running: Multiple loggers"
OUTPUT=$(printf "hi\n<END>\n" | ./output/analyzer 5 logger uppercaser logger)
if echo "$OUTPUT" | grep -q "^\[logger\] hi" && echo "$OUTPUT" | grep -q "^\[logger\] HI"; then
    print_status "Multiple loggers"
    PASSED_TESTS=$((PASSED_TESTS+1))
    TOTAL_TESTS=$((TOTAL_TESTS+1))
else
    print_error "Multiple loggers → expected both '[logger] hi' and '[logger] HI', got: $OUTPUT"
fi

print_info "Running: Logger at both ends"
OUTPUT=$(printf "bye\n<END>\n" | ./output/analyzer 5 logger uppercaser logger flipper logger)
if echo "$OUTPUT" | grep -q "^\[logger\] bye" && echo "$OUTPUT" | grep -q "^\[logger\] BYE" && echo "$OUTPUT" | grep -q "^\[logger\] EYB"; then
    print_status "Logger at both ends"
    PASSED_TESTS=$((PASSED_TESTS+1))
    TOTAL_TESTS=$((TOTAL_TESTS+1))
else
    print_error "Logger at both ends → expected '[logger] bye', '[logger] BYE', '[logger] EYB', got: $OUTPUT"
fi

# ─────────────────────────────────────────────────────────────────────────────
#  7. Typewriter plugin tests
# ─────────────────────────────────────────────────────────────────────────────
print_info "Running: Typewriter basic"
OUTPUT=$(printf "hi\n<END>\n" | ./output/analyzer 5 typewriter)
if echo "$OUTPUT" | grep -q "Pipeline shutdown complete"; then
    print_status "Typewriter basic"
    PASSED_TESTS=$((PASSED_TESTS+1))
    TOTAL_TESTS=$((TOTAL_TESTS+1))
else
    print_error "Typewriter basic → did not complete properly"
fi

print_info "Running: Typewriter in chain"
OUTPUT=$(printf "yo\n<END>\n" | ./output/analyzer 5 uppercaser typewriter logger)
if echo "$OUTPUT" | grep -q "\[logger\] YO" && echo "$OUTPUT" | grep -q "Pipeline shutdown complete"; then
    print_status "Typewriter in chain"
    PASSED_TESTS=$((PASSED_TESTS+1))
    TOTAL_TESTS=$((TOTAL_TESTS+1))
else
    print_error "Typewriter in chain → did not produce expected output"
fi

# ─────────────────────────────────────────────────────────────────────────────
#  Final results
# ─────────────────────────────────────────────────────────────────────────────
echo
print_info "Total tests: $TOTAL_TESTS"
if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
    print_status "All tests passed ✅"
else
    print_error "$((TOTAL_TESTS-PASSED_TESTS)) tests failed ❌"
fi

