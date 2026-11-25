// mapper "in" port: a PSDU to transmit -> service bits, scrambler, convolutional encoder,
// puncturing, interleaver, symbol split, for the encoding chosen by byte 0.
#include "common.h"
#include <gnuradio/blocks/head.h>
#include <gnuradio/blocks/null_sink.h>
#include <ieee802_11/mapper.h>
#include <thread>

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzh::quiet_stdout(); return 0; }

static const int n_dbps[8] = { 24, 36, 48, 72, 96, 144, 192, 216 };

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1)
        return -1;
    int enc = data[0] % 8;
    size_t len = size - 1;
    // The mapper's contract (lib/utils.h MAX_PSDU_SIZE, enforced through MAX_SYM in
    // mapper_impl.cc) is a PSDU of at most 1528 bytes; above it the block drops the
    // packet and waits for the next one forever.
    if (len > 1528)
        return -1;
    // n_sym from IEEE 802.11-2016 17-11: ceil((16 + 8*len + 6) / n_dbps)
    size_t nsym = (16 + 8 * len + 6 + n_dbps[enc] - 1) / n_dbps[enc];

    auto tb = gr::make_top_block("fuzz_mapper");
    auto map = gr::ieee802_11::mapper::make((gr::ieee802_11::Encoding)enc, true);
    auto head = gr::blocks::head::make(1, nsym * 48);
    auto sink = gr::blocks::null_sink::make(1);
    tb->connect(map, 0, head, 0);
    tb->connect(head, 0, sink, 0);
    map->_post(pmt::mp("in"), pmt::cons(pmt::make_dict(), pmt::make_blob(data + 1, len)));
    // mapper is a message-driven source: it never sees the end of the stream, so the
    // flowgraph is stopped once head has passed this packet's symbols through.
    tb->start();
    while (head->nitems_written(0) < nsym * 48)
        std::this_thread::yield();
    tb->stop();
    tb->wait();
    return 0;
}
