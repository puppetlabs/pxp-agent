#ifndef SRC_COMMON_UUID_H_
#define SRC_COMMON_UUID_H_

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <string>

namespace Cthun {
namespace Common {

typedef std::string UUID;

static UUID getUUID() __attribute__((unused));

/// Return a new UUID instance
static UUID getUUID() {
    static boost::uuids::random_generator gen;
    boost::uuids::uuid uuid = gen();
    return boost::uuids::to_string(uuid);
}

}  // namespace Common
}  // namespace Cthun

#endif  // SRC_COMMON_UUID_H_
