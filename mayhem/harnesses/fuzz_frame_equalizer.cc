// frame_equalizer: post-FFT OFDM symbols (64 subcarriers) of one frame tagged
// "wifi_start" by sync_long. Equalizes (LS/LMS/COMB/STA), decodes the SIGNAL field and
// demaps the data symbols, then decode_mac -> parse_mac.
// Layout: byte 0 = equalizer algorithm, byte 1 = frequency-offset tag, rest = sc16 I/Q
// subcarrier values (64 per symbol).
#include "common.h"
#include <gnuradio/blocks/vector_source.h>
#include <ieee802_11/decode_mac.h>
#include <ieee802_11/frame_equalizer.h>
#include <ieee802_11/parse_mac.h>

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzh::quiet_stdout(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2)
        return -1;
    auto algo = (gr::ieee802_11::Equalizer)(data[0] % 4);
    // sync_long tags wifi_start with the estimated phase offset per sample, a small angle
    double off = ((int8_t)data[1]) / 128.0 * 0.1;
    auto s = fuzzh::sc16_samples(data + 2, size - 2);
    // Whole OFDM symbols only: zero-pad the last one (a zero subcarrier is a valid sample), so
    // short inputs still reach the equalizer and libFuzzer can grow its corpus from them.
    s.resize((s.size() + 63) / 64 * 64);
    if (s.empty())
        s.resize(64);

    std::vector<gr::tag_t> tags(1);
    tags[0].offset = 0;
    tags[0].key = pmt::mp("wifi_start");
    tags[0].value = pmt::from_double(off);

    auto tb = gr::make_top_block("fuzz_frame_equalizer");
    auto src = gr::blocks::vector_source_c::make(s, false, 64, tags);
    auto eq = gr::ieee802_11::frame_equalizer::make(algo, 5.89e9, 10e6, false, true);
    auto dec = gr::ieee802_11::decode_mac::make(false, true);
    auto sink = gnuradio::make_block_sptr<fuzzh::collect>();
    tb->connect(src, 0, eq, 0);
    tb->connect(eq, 0, dec, 0);
    tb->msg_connect(dec, "out", sink, "in");
    tb->run();

    auto parse = gr::ieee802_11::parse_mac::make(false, true);
    for (auto& m : sink->drain())
        fuzzh::deliver(parse, "in", m);
    return 0;
}
