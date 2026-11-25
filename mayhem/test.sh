#!/usr/bin/env bash
# mayhem/test.sh — RUN the known-answer selftest that mayhem/build.sh built with normal flags
# (upstream ships no C++ tests). Each case asserts exact decoded/converted bytes.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

runner="$SRC/build-mayhem-test/selftest"
[ -x "$runner" ] || { echo "selftest runner missing: $runner (build.sh bug)" >&2; emit_ctrf selftest 0 1; exit 1; }

out=$("$runner" 2>&1 >/dev/null); rc=$?
echo "$out"
expected="$(printf 'roundtrip_encoding_%d ' 0 1 2 3 4 5 6 7)ether_from_wifi ether_from_tap"
passed=0; failed=0
for t in $expected; do
  if grep -qx "PASS $t" <<<"$out"; then passed=$((passed+1)); else failed=$((failed+1)); fi
done
[ "$rc" -eq 0 ] || [ "$failed" -gt 0 ] || failed=1
emit_ctrf selftest "$passed" "$failed"
