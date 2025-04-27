#ifndef ADDRESS_H
#define ADDRESS_H

// Include the actual engine implementation files first
#include "socketEngine.h"
#include "sharedMemoryEngine.h"

// Then include the protocol and types
#include "protocol.h"
#include "types.h"

// Now that Protocol is fully defined, we can define TheAddress
using TheAddress = TheProtocol::Address;
using TheCommunicator = Communicator<TheProtocol>;

#endif // ADDRESS_H 