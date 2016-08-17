#include <boost/format.hpp>

#include <string>

static boost::format ENVELOPE_FORMAT {
    "{  \"id\" : %1%,"
    "   \"message_type\" : %2%,"
    "   \"expires\" : %3%,"
    "   \"targets\" : [\"client_1\", \"client_2\"],"
    "   \"sender\" : %4%,"
    "   \"destination_report\" : false"
    "}"
};

static boost::format DATA_FORMAT {
    "{  \"transaction_id\" : %1%,"
    "   \"module\" : %2%,"
    "   \"action\" : %3%,"
    "   \"params\" : %4%"
    "}"
};

static boost::format NON_BLOCKING_DATA_FORMAT {
    "{  \"transaction_id\" : %1%,"
    "   \"module\" : %2%,"
    "   \"action\" : %3%,"
    "   \"params\" : %4%,"
    "   \"notify_outcome\" : %5%"
    "}"
};

static const std::string MESSAGE_ID { "123456" };

static const std::string SENDER { "pcp://controller/test_controller" };

static const std::string ENVELOPE_TXT {
    (ENVELOPE_FORMAT % ("\""+MESSAGE_ID+"\"")
                     % "\"test_message\""
                     % "\"2015-06-26T22:57:09Z\""
                     % ("\""+SENDER+"\"")).str() };
