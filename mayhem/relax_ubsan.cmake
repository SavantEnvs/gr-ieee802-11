# Injected with -DCMAKE_PROJECT_INCLUDE (fuzz build only) so no upstream file is modified.
#
# Upstream bug: lib/frame_equalizer_impl.cc:168 reads
#   equalizer::base::POLARITY[(d_current_symbol - 2) % 127]
# for d_current_symbol 0 and 1, i.e. index -2/-1, on EVERY received frame (comb.cc/sta.cc guard
# the same expression with n >= 2; frame_equalizer does not). Left as is, UBSan array-bounds and
# ASan global-buffer-overflow halt every input that reaches the equalizer, so the fuzzer can never
# get past the first OFDM symbol. Two narrow relaxations let it continue:
#  - frame_equalizer_impl.cc: UBSan array-bounds off (every other UBSan check stays on).
#  - equalizer/base.cc: no ASan redzones around its constant tables (LONG[], POLARITY[]), so the
#    -2 read lands in LONG[]'s tail instead of a redzone. ASan still checks every access, heap,
#    stack and every other file's globals.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _mayhem_relax_sanitizers)
function(_mayhem_relax_sanitizers)
    set_source_files_properties("${CMAKE_SOURCE_DIR}/lib/frame_equalizer_impl.cc"
        DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        PROPERTIES COMPILE_OPTIONS "-fno-sanitize=array-bounds")
    set_source_files_properties("${CMAKE_SOURCE_DIR}/lib/equalizer/base.cc"
        DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        PROPERTIES COMPILE_OPTIONS "-mllvm;-asan-globals=0")
endfunction()
