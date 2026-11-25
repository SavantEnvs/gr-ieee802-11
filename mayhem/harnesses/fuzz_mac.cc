// mac "app in": a payload from the application -> 802.11 MAC data frame + FCS.
// The block's other input, "phy in" (received MPDUs), has no target: its handler only strips
// a 24-byte header with no length check (lib/mac.cc:87, make_blob(mpdu + 24, blob_length - 24)),
// so any received frame under 24 bytes, the empty one included, underflows the length and
// throws std::length_error. A raw "phy in" target hits that on its first input (Mayhem defect
// 3736467) and cannot fuzz; skipping short frames would hide the bug. It stays an upstream
// finding until upstream checks the length.
#include "common.h"
#include <ieee802_11/mac.h>
#include <stdexcept>

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzh::quiet_stdout(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::vector<uint8_t> src{ 0x23, 0x23, 0x23, 0x23, 0x23, 0x23 };
    std::vector<uint8_t> dst{ 0x42, 0x42, 0x42, 0x42, 0x42, 0x42 };
    std::vector<uint8_t> bss{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    auto blk = gr::ieee802_11::mac::make(src, dst, bss);
    try {
        fuzzh::deliver(blk, "app in", pmt::cons(pmt::make_dict(), pmt::make_blob(data, size)));
    } catch (const std::invalid_argument&) {
        // documented reject: app in throws for a payload above MAX_PAYLOAD_SIZE
    }
    return 0;
}
