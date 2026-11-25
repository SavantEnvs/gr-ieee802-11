// decode_mac: demodulated OFDM data symbols (48 bytes per symbol, one byte per carrier,
// n_bpsc bits each) plus the frame_equalizer tags "encoding" / "frame bytes". Runs
// deinterleave, Viterbi, descramble, CRC32 and hands a valid PSDU to parse_mac.
// Layout: byte 0 = encoding (0..7, the SIGNAL rate values frame_equalizer emits),
// bytes 1-2 = frame bytes (LE), rest = symbols (truncated to whole 48-byte symbols).
#include "common.h"
#include <gnuradio/blocks/vector_source.h>
#include <ieee802_11/decode_mac.h>
#include <ieee802_11/parse_mac.h>

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzh::quiet_stdout(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 3 + 48)
        return -1;
    uint64_t enc = data[0] % 8;
    uint64_t len = data[1] | (data[2] << 8);
    size_t nsym = (size - 3) / 48;
    std::vector<uint8_t> sym(data + 3, data + 3 + nsym * 48);

    std::vector<gr::tag_t> tags(2);
    tags[0].offset = 0;
    tags[0].key = pmt::mp("frame bytes");
    tags[0].value = pmt::from_uint64(len);
    tags[1].offset = 0;
    tags[1].key = pmt::mp("encoding");
    tags[1].value = pmt::from_uint64(enc);

    auto tb = gr::make_top_block("fuzz_decode_mac");
    auto src = gr::blocks::vector_source_b::make(sym, false, 48, tags);
    auto dec = gr::ieee802_11::decode_mac::make(false, true);
    auto sink = gnuradio::make_block_sptr<fuzzh::collect>();
    tb->connect(src, 0, dec, 0);
    tb->msg_connect(dec, "out", sink, "in");
    tb->run();

    auto parse = gr::ieee802_11::parse_mac::make(false, true);
    for (auto& m : sink->drain())
        fuzzh::deliver(parse, "in", m);
    return 0;
}
