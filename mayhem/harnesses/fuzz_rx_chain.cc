// The full receive chain of examples/wifi_phy_hier.grc on raw baseband samples:
// short-preamble autocorrelation -> sync_short (frame detection, coarse CFO) ->
// sync_long (long-preamble correlation, symbol alignment) -> FFT -> frame_equalizer ->
// decode_mac -> parse_mac. Input: sc16 I/Q samples. Byte 0 picks the equalizer.
#include "common.h"
#include <gnuradio/blocks/complex_to_mag.h>
#include <gnuradio/blocks/complex_to_mag_squared.h>
#include <gnuradio/blocks/conjugate_cc.h>
#include <gnuradio/blocks/delay.h>
#include <gnuradio/blocks/divide.h>
#include <gnuradio/blocks/moving_average.h>
#include <gnuradio/blocks/multiply.h>
#include <gnuradio/blocks/stream_to_vector.h>
#include <gnuradio/blocks/vector_source.h>
#include <gnuradio/fft/fft_v.h>
#include <gnuradio/fft/window.h>
#include <ieee802_11/decode_mac.h>
#include <ieee802_11/frame_equalizer.h>
#include <ieee802_11/parse_mac.h>
#include <ieee802_11/sync_long.h>
#include <ieee802_11/sync_short.h>

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzh::quiet_stdout(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1 + 4)
        return -1;
    const int window = 48, sync_length = 320;
    auto algo = (gr::ieee802_11::Equalizer)(data[0] % 4);
    auto s = fuzzh::sc16_samples(data + 1, size - 1);

    using namespace gr::blocks;
    auto tb = gr::make_top_block("fuzz_rx_chain");
    auto src = vector_source_c::make(s, false);
    auto d16 = delay::make(sizeof(gr_complex), 16);
    auto conj = conjugate_cc::make();
    auto mul = multiply_cc::make();
    auto avg_c = moving_average_cc::make(window, 1, 4000);
    auto mag = complex_to_mag::make();
    auto mag2 = complex_to_mag_squared::make();
    auto avg_p = moving_average_ff::make(window + 16, 1, 4000);
    auto div = divide_ff::make();
    auto ss = gr::ieee802_11::sync_short::make(0.56, 2, false, true);
    auto dl = delay::make(sizeof(gr_complex), sync_length);
    auto sl = gr::ieee802_11::sync_long::make(sync_length, false, true);
    auto s2v = stream_to_vector::make(sizeof(gr_complex), 64);
    auto fft = gr::fft::fft_v<gr_complex, true>::make(64, gr::fft::window::rectangular(64), true, 1);
    auto eq = gr::ieee802_11::frame_equalizer::make(algo, 5.89e9, 10e6, false, true);
    auto dec = gr::ieee802_11::decode_mac::make(false, true);
    auto sink = gnuradio::make_block_sptr<fuzzh::collect>();

    tb->connect(src, 0, d16, 0);
    tb->connect(d16, 0, conj, 0);
    tb->connect(src, 0, mul, 0);
    tb->connect(conj, 0, mul, 1);
    tb->connect(mul, 0, avg_c, 0);
    tb->connect(avg_c, 0, mag, 0);
    tb->connect(src, 0, mag2, 0);
    tb->connect(mag2, 0, avg_p, 0);
    tb->connect(mag, 0, div, 0);
    tb->connect(avg_p, 0, div, 1);
    tb->connect(d16, 0, ss, 0);
    tb->connect(avg_c, 0, ss, 1);
    tb->connect(div, 0, ss, 2);
    tb->connect(ss, 0, sl, 0);
    tb->connect(ss, 0, dl, 0);
    tb->connect(dl, 0, sl, 1);
    tb->connect(sl, 0, s2v, 0);
    tb->connect(s2v, 0, fft, 0);
    tb->connect(fft, 0, eq, 0);
    tb->connect(eq, 0, dec, 0);
    tb->msg_connect(dec, "out", sink, "in");
    tb->run();

    auto parse = gr::ieee802_11::parse_mac::make(false, true);
    for (auto& m : sink->drain())
        fuzzh::deliver(parse, "in", m);
    return 0;
}
