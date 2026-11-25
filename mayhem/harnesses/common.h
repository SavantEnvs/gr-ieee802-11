// Shared helpers for the gr-ieee802-11 fuzz harnesses.
#pragma once
#include <gnuradio/basic_block.h>
#include <gnuradio/block.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/top_block.h>
#include <gnuradio/tags.h>
#include <pmt/pmt.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <complex>
#include <vector>

namespace fuzzh {

// gr::basic_block::dispatch_msg is protected; this accessor reaches the block's own
// registered message handler synchronously, exactly as the scheduler would.
struct acc : gr::basic_block {
    using gr::basic_block::dispatch_msg;
};
inline void deliver(gr::basic_block_sptr blk, const char* port, pmt::pmt_t msg)
{
    (blk.get()->*(&acc::dispatch_msg))(pmt::mp(port), msg);
}

// Message sink with no handler: messages queue up and are drained after the flowgraph
// has finished, so nothing races the scheduler's shutdown.
class collect : public gr::block
{
public:
    collect()
        : gr::block("fuzz_collect", gr::io_signature::make(0, 0, 0),
                    gr::io_signature::make(0, 0, 0))
    {
        message_port_register_in(pmt::mp("in"));
    }
    std::vector<pmt::pmt_t> drain()
    {
        std::vector<pmt::pmt_t> v;
        for (;;) {
            pmt::pmt_t m = delete_head_nowait(pmt::mp("in"));
            if (!m.get())
                break;
            v.push_back(m);
        }
        return v;
    }
};

// Blocks print their debug output to stdout; keep it out of the fuzzer's way.
inline void quiet_stdout() { (void)!freopen("/dev/null", "w", stdout); }

// Received samples come from an SDR as interleaved signed 16-bit I/Q (sc16), so they are
// always finite and bounded; map fuzz bytes the same way.
inline std::vector<gr_complex> sc16_samples(const uint8_t* d, size_t n)
{
    std::vector<gr_complex> v(n / 4);
    for (size_t k = 0; k < v.size(); k++) {
        int16_t i, q;
        std::memcpy(&i, d + 4 * k, 2);
        std::memcpy(&q, d + 4 * k + 2, 2);
        v[k] = gr_complex(i / 32768.0f, q / 32768.0f);
    }
    return v;
}

} // namespace fuzzh
