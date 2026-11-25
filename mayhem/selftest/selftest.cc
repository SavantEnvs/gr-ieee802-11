// Known-answer functional tests for gr-ieee802-11 (upstream ships no C++ tests).
//  * roundtrip_<enc>: a PSDU (with FCS) goes through the TX mapper (scrambler, encoder,
//    puncturing, interleaver) and back through decode_mac (deinterleaver, Viterbi,
//    descrambler, CRC); the decoded payload must equal the original byte for byte.
//  * ether_from_wifi / ether_from_tap: ether_encap's 802.11 <-> Ethernet conversion
//    must produce the exact expected frames.
// Prints "PASS <name>" / "FAIL <name>" per test.
#include "../harnesses/common.h"
#include <boost/crc.hpp>
#include <gnuradio/blocks/head.h>
#include <gnuradio/blocks/vector_sink.h>
#include <gnuradio/blocks/vector_source.h>
#include <ieee802_11/decode_mac.h>
#include <ieee802_11/ether_encap.h>
#include <ieee802_11/mapper.h>
#include <iostream>
#include <string>
#include <thread>

static int failures = 0;
static void report(const std::string& name, bool ok)
{
    std::cerr << (ok ? "PASS " : "FAIL ") << name << std::endl;
    if (!ok)
        failures++;
}

static const int n_dbps[8] = { 24, 36, 48, 72, 96, 144, 192, 216 };

static std::vector<uint8_t> roundtrip(int enc, const std::vector<uint8_t>& psdu)
{
    size_t nsym = (16 + 8 * psdu.size() + 6 + n_dbps[enc] - 1) / n_dbps[enc];
    auto tb = gr::make_top_block("tx");
    auto map = gr::ieee802_11::mapper::make((gr::ieee802_11::Encoding)enc, false);
    auto head = gr::blocks::head::make(1, nsym * 48);
    auto vs = gr::blocks::vector_sink_b::make(1);
    tb->connect(map, 0, head, 0);
    tb->connect(head, 0, vs, 0);
    map->_post(pmt::mp("in"), pmt::cons(pmt::make_dict(), pmt::make_blob(psdu.data(), psdu.size())));
    tb->start(); // mapper never ends its stream; stop once the sink holds the whole packet
    while (vs->data().size() < nsym * 48)
        std::this_thread::yield();
    tb->stop();
    tb->wait();
    std::vector<uint8_t> sym = vs->data();
    if (sym.size() != nsym * 48)
        return {};

    std::vector<gr::tag_t> tags(2);
    tags[0].offset = 0;
    tags[0].key = pmt::mp("frame bytes");
    tags[0].value = pmt::from_uint64(psdu.size());
    tags[1].offset = 0;
    tags[1].key = pmt::mp("encoding");
    tags[1].value = pmt::from_uint64(enc);
    auto rx = gr::make_top_block("rx");
    auto src = gr::blocks::vector_source_b::make(sym, false, 48, tags);
    auto dec = gr::ieee802_11::decode_mac::make(false, false);
    auto sink = gnuradio::make_block_sptr<fuzzh::collect>();
    rx->connect(src, 0, dec, 0);
    rx->msg_connect(dec, "out", sink, "in");
    rx->run();
    auto msgs = sink->drain();
    if (msgs.size() != 1)
        return {};
    pmt::pmt_t blob = pmt::cdr(msgs[0]);
    const uint8_t* p = (const uint8_t*)pmt::blob_data(blob);
    return std::vector<uint8_t>(p, p + pmt::blob_length(blob));
}

static std::vector<uint8_t> port_out(gr::basic_block_sptr blk, const char* in_port,
                                     const char* out_port, const std::vector<uint8_t>& in)
{
    auto sink = gnuradio::make_block_sptr<fuzzh::collect>();
    blk->message_port_sub(pmt::mp(out_port), pmt::cons(sink->alias_pmt(), pmt::mp("in")));
    fuzzh::deliver(blk, in_port, pmt::cons(pmt::PMT_NIL, pmt::make_blob(in.data(), in.size())));
    blk->message_port_unsub(pmt::mp(out_port), pmt::cons(sink->alias_pmt(), pmt::mp("in")));
    auto msgs = sink->drain();
    if (msgs.size() != 1)
        return {};
    pmt::pmt_t blob = pmt::cdr(msgs[0]);
    const uint8_t* p = (const uint8_t*)pmt::blob_data(blob);
    return std::vector<uint8_t>(p, p + pmt::blob_length(blob));
}

int main()
{
    // A 24-byte 802.11 data header, LLC/SNAP, and an ASCII payload, then the FCS.
    std::vector<uint8_t> frame = { 0x08, 0x00, 0x2e, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
                                   0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0xff, 0xff, 0xff, 0xff,
                                   0xff, 0xff, 0x10, 0x00, 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00,
                                   0x08, 0x00 };
    const std::string text = "Hello from gr-ieee802-11 selftest";
    frame.insert(frame.end(), text.begin(), text.end());
    boost::crc_32_type crc;
    crc.process_bytes(frame.data(), frame.size());
    uint32_t fcs = crc.checksum();
    std::vector<uint8_t> psdu = frame;
    for (int k = 0; k < 4; k++)
        psdu.push_back((fcs >> (8 * k)) & 0xff);

    for (int enc = 0; enc < 8; enc++)
        report("roundtrip_encoding_" + std::to_string(enc), roundtrip(enc, psdu) == frame);

    // 802.11 data frame -> Ethernet: dest = addr1, src = addr2, type IP, payload after 32 bytes
    {
        auto enc = gr::ieee802_11::ether_encap::make(false);
        std::vector<uint8_t> want = { 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x23, 0x23,
                                      0x23, 0x23, 0x23, 0x23, 0x08, 0x00 };
        want.insert(want.end(), text.begin(), text.end());
        report("ether_from_wifi", port_out(enc, "from wifi", "to tap", frame) == want);
    }
    // Ethernet IP frame -> LLC/SNAP-prefixed payload
    {
        auto enc = gr::ieee802_11::ether_encap::make(false);
        std::vector<uint8_t> in = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0x08, 0x00, 'x', 'y', 'z' };
        std::vector<uint8_t> want = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 'x', 'y', 'z' };
        report("ether_from_tap", port_out(enc, "from tap", "to wifi", in) == want);
    }
    std::cerr << "SUMMARY failed=" << failures << std::endl;
    return failures ? 1 : 0;
}
