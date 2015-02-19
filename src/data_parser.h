#ifndef CTHUN_SRC_DATA_PARSER_H_
#define CTHUN_SRC_DATA_PARSER_H_

#include "src/message.h"
#include "src/data_container.h"
#include "src/schemas.h"

namespace CthunAgent {

namespace DataParser {

DataContainer parseAndValidateChunk(const MessageChunk& chunk,
                                    CthunContentFormat format =
                                            CthunContentFormat::json);

}  // namespace DataParser

}  // namespace CthunAgent

#endif  // CTHUN_SRC_DATA_PARSER_H_
