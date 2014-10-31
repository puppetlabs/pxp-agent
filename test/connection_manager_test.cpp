#include "test/test.h"
#include "test/connection_manager_certificates.h"

#include "src/websocket/connection_manager.h"
#include "src/websocket/errors.h"
#include "src/common/uuid.h"
#include "src/common/file_utils.h"

#include <string>
#include <vector>
#include <iostream>

namespace Cthun {
namespace WebSocket {

//
// Base class
//

class ConnectionManagerTest : public ::testing::Test {
  public:
    static const std::string CA_PATH;
    static const std::string CRT_PATH;
    static const std::string KEY_PATH;

    void writeCertificates() {
        try {
            Cthun:: Common::FileUtils::writeToFile(CA, CA_PATH);
            Cthun::Common::FileUtils::writeToFile(CRT, CRT_PATH);
            Cthun::Common::FileUtils::writeToFile(KEY, KEY_PATH);
        } catch (Cthun::Common::FileUtils::file_error& e) {
            ADD_FAILURE() << "FAILED TO WRITE CERTIFICATES: " << e.what();
        }
    }

  protected:
    virtual void SetUp() {}

    virtual void TearDown() {
        try {
            Cthun::Common::FileUtils::removeFile(CA_PATH);
            Cthun::Common::FileUtils::removeFile(CRT_PATH);
            Cthun::Common::FileUtils::removeFile(KEY_PATH);
        } catch (Cthun::Common::FileUtils::file_error) {
            ADD_FAILURE() << "FAILED TO REMOVE CERTIFICATES";
        }

        CONNECTION_MANAGER.resetEndpoint();
    }
};

static const Cthun::Common::UUID TEST_UUID { Common::getUUID() };

const std::string ConnectionManagerTest::CA_PATH { "./test_ca_" + TEST_UUID };
const std::string ConnectionManagerTest::CRT_PATH { "./test_crt_" + TEST_UUID };
const std::string ConnectionManagerTest::KEY_PATH { "./test_key_" + TEST_UUID };

//
// Tests
//

// The ConnectionManager singleton can instantiate an Endpoint
TEST_F(ConnectionManagerTest, ConfigureSecureEndpoint) {
    ConnectionManagerTest::writeCertificates();
    try {
        CONNECTION_MANAGER.configureSecureEndpoint(
            ConnectionManagerTest::CA_PATH,
            ConnectionManagerTest::CRT_PATH,
            ConnectionManagerTest::KEY_PATH);
    } catch (std::exception& e) {
        FAIL() << "Unexpected exception: " << e.what();
        return;
    }
}

// Should fail when instantiating an Endpoint with missing certs
TEST_F(ConnectionManagerTest, ConfigureSecureEndpoint_Failure) {
    EXPECT_THROW(CONNECTION_MANAGER.configureSecureEndpoint(
                     ConnectionManagerTest::CA_PATH,
                     ConnectionManagerTest::CRT_PATH,
                     ConnectionManagerTest::KEY_PATH),
                 endpoint_error);
}

// Should fail when configuring Endpoint after instantiating it
TEST_F(ConnectionManagerTest, ConfigureSecureEndpoint_FailureBis) {
    ConnectionManagerTest::writeCertificates();
    Connection::Ptr p {
        CONNECTION_MANAGER.createConnection("ws://127.0.0.1/cthun/") };
    CONNECTION_MANAGER.open(p);
    EXPECT_THROW(CONNECTION_MANAGER.configureSecureEndpoint(
                     ConnectionManagerTest::CA_PATH,
                     ConnectionManagerTest::CRT_PATH,
                     ConnectionManagerTest::KEY_PATH),
                 endpoint_error);
}

// Can open a connection
TEST_F(ConnectionManagerTest, Open) {
    try {
        ConnectionManagerTest::writeCertificates();
        Connection::Ptr p {
            CONNECTION_MANAGER.createConnection("wss://127.0.0.1/cthun/") };
        CONNECTION_MANAGER.configureSecureEndpoint(
            ConnectionManagerTest::CA_PATH,
            ConnectionManagerTest::CRT_PATH,
            ConnectionManagerTest::KEY_PATH);
        CONNECTION_MANAGER.open(p);
    } catch (std::exception& e) {
        FAIL() << "Unexpected exception: " << e.what();
        return;
    }
}

// Can configure an Endpoint after resetting
TEST_F(ConnectionManagerTest, Reset) {
    try {
        ConnectionManagerTest::writeCertificates();
        Connection::Ptr p {
            CONNECTION_MANAGER.createConnection("wss://127.0.0.1/cthun/") };
        CONNECTION_MANAGER.open(p);
        CONNECTION_MANAGER.resetEndpoint();
        CONNECTION_MANAGER.configureSecureEndpoint(
            ConnectionManagerTest::CA_PATH,
            ConnectionManagerTest::CRT_PATH,
            ConnectionManagerTest::KEY_PATH);
        CONNECTION_MANAGER.open(p);
    } catch (std::exception& e) {
        FAIL() << "Unexpected exception: " << e.what();
        return;
    }
}

// Can get the list of connection IDs
TEST_F(ConnectionManagerTest, GetConnectionIDs) {
    ConnectionManagerTest::writeCertificates();
    Connection::Ptr p {
        CONNECTION_MANAGER.createConnection("ws://127.0.0.1/cthun/") };
    // the connection must be opened before being tracked by the endpoint
    unsigned long expected_size { 0 };  // NOLINT(runtime/int): googletest type
    EXPECT_EQ(expected_size, CONNECTION_MANAGER.getConnectionIDs().size());

    CONNECTION_MANAGER.open(p);
    EXPECT_EQ(p->getID(), CONNECTION_MANAGER.getConnectionIDs()[0]);
}

// Can close connections
TEST_F(ConnectionManagerTest, CloseAllConnections) {
    ConnectionManagerTest::writeCertificates();
    Connection::Ptr p1 {
        CONNECTION_MANAGER.createConnection("ws://127.0.0.1/cthun_1/") };
    Connection::Ptr p2 {
        CONNECTION_MANAGER.createConnection("ws://127.0.0.1/cthun_2/") };
    CONNECTION_MANAGER.open(p1);
    CONNECTION_MANAGER.open(p2);
    unsigned long expected_size { 2 };  // NOLINT(runtime/int)
    EXPECT_EQ(expected_size, CONNECTION_MANAGER.getConnectionIDs().size());

    CONNECTION_MANAGER.closeAllConnections();
    expected_size = 0;
    EXPECT_EQ(expected_size, CONNECTION_MANAGER.getConnectionIDs().size());
}

}  // namespace WebSocket
}  // namespace Cthun
