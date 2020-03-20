#include "certs.hpp"

#include "root_path.hpp"

std::string getCaPath() {
    static const std::string ca { std::string { PXP_AGENT_ROOT_PATH }
                                  + "/lib/tests/resources/config/ca_crt.pem" };
    return ca;
}

std::string getCertPath() {
    static const std::string cert { std::string { PXP_AGENT_ROOT_PATH }
                                    + "/lib/tests/resources/config/test_crt.pem" };
    return cert;
}

std::string getKeyPath() {
    static const std::string key { std::string { PXP_AGENT_ROOT_PATH }
                                   + "/lib/tests/resources/config/test_key.pem" };
    return key;
}

std::string getNotExistentFilePath() {
    static const std::string tmp { std::string { PXP_AGENT_ROOT_PATH }
                                   + "/lib/tests/resources/config/nothing.here" };
    return tmp;
}
