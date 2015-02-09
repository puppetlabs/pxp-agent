#ifndef CTHUN_SRC_MESSAGE_SERIALIZATION_H_
#define CTHUN_SRC_MESSAGE_SERIALIZATION_H_

#include <vector>
#include <stdint.h>  // uint8_t
#include <netinet/in.h>  // endianess functions: htonl, ntohl

namespace CthunAgent {


// typedef std::vector<char> MessageBuffer;
typedef std::vector<uint8_t> MessageBuffer;

// Approach based on https://github.com/motonacciu/meta-serialization
// and on StackOverflow's question
// https://stackoverflow.com/questions/5303617
//          /constructing-and-sending-binary-data-over-network


// Integer

inline void serialize(const uint32_t& number, MessageBuffer& buffer);

// String


// Function objects


}  // namespace CthunAgent

#endif  // CTHUN_SRC_MESSAGE_SERIALIZATION_H_
