#ifndef SRC_UUID_H_
#define SRC_UUID_H_

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <string>

namespace CthunAgent {
namespace UUID {

static std::string getUUID() __attribute__((unused));

/// Return a new UUID string
static std::string getUUID() {
    static boost::uuids::random_generator gen;
    boost::uuids::uuid uuid = gen();
    return boost::uuids::to_string(uuid);
}

}  // namespace UUID
}  // namespace CthunAgent


#endif  // SRC_UUID_H_
