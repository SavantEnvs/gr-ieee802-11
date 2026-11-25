// ether_encap: "from wifi" (received 802.11 data frame -> Ethernet) and "from tap"
// (Ethernet frame from the TAP device -> 802.11 LLC/SNAP). Byte 0 picks the port.
#include "common.h"
#include <ieee802_11/ether_encap.h>

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzh::quiet_stdout(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1)
        return -1;
    auto blk = gr::ieee802_11::ether_encap::make(true);
    const char* port = (data[0] & 1) ? "from tap" : "from wifi";
    fuzzh::deliver(blk, port, pmt::cons(pmt::PMT_NIL, pmt::make_blob(data + 1, size - 1)));
    return 0;
}
