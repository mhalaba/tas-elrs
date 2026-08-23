#include "TasFailsafe.h"

TasWatchdogVerdict_e TasWatchdogEvaluate(
    uint32_t nowMs,
    uint32_t lastValidPacketMs,
    bool connectionEstablished)
{
    uint32_t silence = nowMs - lastValidPacketMs;

    // Link was up and RF went quiet beyond tolerance => silicon wedge.
    if (connectionEstablished && silence > TAS_RADIO_REINIT_MS)
    {
        return TAS_WD_RADIO_WEDGE;
    }

    // Was connected before but lost for 2s..60s => likely RF-level wedge
    // rather than plain out-of-range; beyond 60s assume normal loss so a
    // bench unit without TX does not reinit forever.
    if (!connectionEstablished && lastValidPacketMs != 0 &&
        silence > TAS_RADIO_REINIT_MS * 2 && silence <= 60000)
    {
        return TAS_WD_RADIO_WEDGE;
    }

    // No packets this session, or plain quiet period: normal RF loss path.
    if (lastValidPacketMs == 0 || silence > TAS_RADIO_REINIT_MS)
    {
        return TAS_WD_LINK_LOST;
    }
    return TAS_WD_OK;
}
