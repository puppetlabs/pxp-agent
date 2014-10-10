#include "test/test.h"

#include "src/websocket/connection.h"

#include <string>

namespace Cthun {
namespace WebSocket {

//
// Base class
//

class ConnectionTest : public ::testing::Test {
  public:
  protected:
    virtual void SetUp() {}

    virtual void TearDown() {}
};

static const std::string TEST_URL { "wss://127.0.0.1/chtun/" };

//
// Tests
//

// Can instantiate a Connection
TEST_F(ConnectionTest, CreateConnection) {
    try {
        Connection connection { TEST_URL };
    } catch (std::exception& e) {
        FAIL() << "Unexpected exception: " << e.what();
        return;
    }
}

// Can get a shared_ptr to a new Connection
TEST_F(ConnectionTest, GetConnectionPointer) {
    try {
        Connection::Ptr connection_ptr { new Connection(TEST_URL) };
    } catch (std::exception& e) {
        FAIL() << "Unexpected exception: " << e.what();
        return;
    }
}

// Can get the url
TEST_F(ConnectionTest, GetURL) {
    Connection::Ptr connection_ptr { new Connection(TEST_URL) };
    EXPECT_EQ(TEST_URL, connection_ptr->getURL());
}

// Can get the Connection_ID
TEST_F(ConnectionTest, GetID) {
    try {
        Connection::Ptr connection_ptr { new Connection(TEST_URL) };
        Connection_ID id { connection_ptr->getID() };
    } catch (std::exception& e) {
        FAIL() << "Unexpected exception: " << e.what();
        return;
    }
}

// Can get the state; the initial state is 'connecting'
TEST_F(ConnectionTest, ConnectionHandle) {
    Connection::Ptr connection_ptr { new Connection(TEST_URL) };
    EXPECT_EQ(Connection_State_Values::connecting, connection_ptr->getState());
}

}  // namespace WebSocket
}  // namespace Cthun
