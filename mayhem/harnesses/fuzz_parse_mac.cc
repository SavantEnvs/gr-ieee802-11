// parse_mac "in" port: a received 802.11 MPDU (management / control / data frame) as a PDU.
#include "common.h"
#include <ieee802_11/parse_mac.h>

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzh::quiet_stdout(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // debug=true exercises the frame printers (SSID, ASCII payload); a fresh block per
    // input keeps the sequence-number / FER tracking state from leaking between inputs.
    auto blk = gr::ieee802_11::parse_mac::make(false, true);
    fuzzh::deliver(blk, "in", pmt::cons(pmt::make_dict(), pmt::make_blob(data, size)));
    return 0;
}
