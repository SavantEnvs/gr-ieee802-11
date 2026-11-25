#!/usr/bin/env bash
# mayhem/build.sh — gr-ieee802-11: build the module with upstream's CMake twice (sanitized
# for the fuzzers, normal flags for the selftest), then the harnesses and the selftest.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS
cd "$SRC"

# Both build dirs match upstream's `build*/` .gitignore entry, so `git clean -ffdX` removes them.
FUZZ_BUILD="$SRC/build-mayhem-fuzz"
TEST_BUILD="$SRC/build-mayhem-test"

# 1) The module, instrumented (CMAKE_BUILD_TYPE=None: only our flags; asserts stay on).
#    UBSan nonnull-attribute is relaxed for the module only: an empty PDU has a NULL
#    pmt::blob_data, and mac.cc:163 / the other blocks memcpy(dst, NULL, 0) from it, which
#    is benign but would halt every run on libFuzzer's first (empty) input. ASan and every
#    other UBSan check stay on and halting.
#    -fsanitize=fuzzer-no-link (always, even with empty SANITIZER_FLAGS) gives the module and the
#    harness TUs SanCov edges; -fsanitize=fuzzer on the link line alone instruments nothing.
COV_FLAGS="-fsanitize=fuzzer-no-link"
LIB_SAN_FLAGS="$SANITIZER_FLAGS $COV_FLAGS"
case "$SANITIZER_FLAGS" in *undefined*) LIB_SAN_FLAGS="$LIB_SAN_FLAGS -fno-sanitize=nonnull-attribute" ;; esac
#    UBSan array-bounds on lib/frame_equalizer_impl.cc and ASan global redzones on
#    lib/equalizer/base.cc are relaxed (those two files only, mayhem/relax_ubsan.cmake):
#    its POLARITY[(d_current_symbol - 2) % 127] reads index -2/-1 on every frame (an upstream bug).
cmake -S "$SRC" -B "$FUZZ_BUILD" -DENABLE_PYTHON=OFF -DENABLE_DOXYGEN=OFF -DCMAKE_BUILD_TYPE=None \
      -DCMAKE_PROJECT_INCLUDE="$SRC/mayhem/relax_ubsan.cmake" \
      -DCMAKE_CXX_FLAGS="-O1 $LIB_SAN_FLAGS $DEBUG_FLAGS" \
      -DCMAKE_C_FLAGS="-O1 $LIB_SAN_FLAGS $DEBUG_FLAGS"
cmake --build "$FUZZ_BUILD" -j"$MAYHEM_JOBS" --target gnuradio-ieee802_11

# 2) The module with the project's normal flags, for the functional selftest.
cmake -S "$SRC" -B "$TEST_BUILD" -DENABLE_PYTHON=OFF -DENABLE_DOXYGEN=OFF -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" \
      -DCMAKE_SHARED_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build "$TEST_BUILD" -j"$MAYHEM_JOBS" --target gnuradio-ieee802_11

GR_LIBS="-lgnuradio-fft -lgnuradio-blocks -lgnuradio-runtime -lgnuradio-pmt -lspdlog -lfmt -lboost_thread -lpthread -lm"
INC="-I$SRC/include -I$SRC/mayhem/harnesses"

# 3) Harnesses: libFuzzer binary + standalone reproducer each, LSan off.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$FUZZ_BUILD/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$FUZZ_BUILD/standalone_main.o"
for t in parse_mac ether_encap mac decode_mac frame_equalizer mapper rx_chain; do
  $CXX $SANITIZER_FLAGS $COV_FLAGS $DEBUG_FLAGS -O1 $INC -c "$SRC/mayhem/harnesses/fuzz_$t.cc" -o "$FUZZ_BUILD/fuzz_$t.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$FUZZ_BUILD/fuzz_$t.o" "$FUZZ_BUILD/lsan_off.o" \
      -L"$FUZZ_BUILD/lib" -lgnuradio-ieee802_11 -Wl,-rpath,"$FUZZ_BUILD/lib" $GR_LIBS -o "$SRC/fuzz_$t"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$FUZZ_BUILD/fuzz_$t.o" "$FUZZ_BUILD/standalone_main.o" "$FUZZ_BUILD/lsan_off.o" \
      -L"$FUZZ_BUILD/lib" -lgnuradio-ieee802_11 -Wl,-rpath,"$FUZZ_BUILD/lib" $GR_LIBS -o "$SRC/fuzz_$t-standalone"
done

# 4) Selftest, normal flags, against the normal build.
$CXX -O2 $COVERAGE_FLAGS $INC "$SRC/mayhem/selftest/selftest.cc" \
    -L"$TEST_BUILD/lib" -lgnuradio-ieee802_11 -Wl,-rpath,"$TEST_BUILD/lib" $GR_LIBS -o "$TEST_BUILD/selftest"
