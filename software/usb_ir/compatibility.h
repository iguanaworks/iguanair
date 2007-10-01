#ifndef _COMPATIBILITY_
#define _COMPATIBILITY_

bool translateClient(dataPacket *packet, uint16_t version, bool copyTo);
bool translateDevice(dataPacket *packet, uint16_t version, bool copyTo);

#endif
