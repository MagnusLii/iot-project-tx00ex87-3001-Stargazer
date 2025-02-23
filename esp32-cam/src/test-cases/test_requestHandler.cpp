#include "test_requestHandler.hpp"
#include "requestHandler.hpp"
#include "sd-card.hpp"
#include "wireless.hpp"

void test_createImagePOSTRequest() {
    auto sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    auto wirelessHandler = std::make_shared<WirelessHandler>(sdcardHandler.get());
    auto requestHandler =
        std::make_shared<RequestHandler>("example.com", "80", "token123", wirelessHandler, sdcardHandler);

    std::string request;
    std::string base64_image_data = "base64_image_data";
    RequestHandlerReturnCode ret = requestHandler->createImagePOSTRequest(&request, 1, base64_image_data);
    TEST_ASSERT_EQUAL(RequestHandlerReturnCode::SUCCESS, ret);

    std::string content = "{"
                          "\"token\":\"token123\","
                          "\"id\":1,"
                          "\"data\":\"base64_image_data\""
                          "}\r\n";

    std::string expected = "POST "
                           "/api/upload"
                           " HTTP/1.0\r\n"
                           "Host: example.com:80\r\n"
                           "User-Agent: esp-idf/1.0 esp32\r\n"
                           "Connection: keep-alive\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: " +
                           std::to_string(content.length()) +
                           "\r\n"
                           "\r\n" +
                           content;

    TEST_ASSERT_EQUAL(RequestHandlerReturnCode::SUCCESS, ret);
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), request.c_str());

    requestHandler.reset();
    wirelessHandler.reset();
    sdcardHandler.reset();
}

void test_createUserInstructionsGETRequest() {
    auto sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    auto wirelessHandler = std::make_shared<WirelessHandler>(sdcardHandler.get());
    auto requestHandler =
        std::make_shared<RequestHandler>("example.com", "80", "token123", wirelessHandler, sdcardHandler);

    std::string request;
    requestHandler->createUserInstructionsGETRequest(&request);

    std::string expected = "GET "
                           "/api/command?token=token123"
                           " HTTP/1.0\r\n"
                           "Host: example.com:80\r\n"
                           "User-Agent: esp-idf/1.0 esp32\r\n"
                           "Connection: keep-alive\r\n"
                           "\r\n";

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), request.c_str());

    requestHandler.reset();
    wirelessHandler.reset();
    sdcardHandler.reset();
}

void test_createTimestampGETRequest() {
    auto sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    auto wirelessHandler = std::make_shared<WirelessHandler>(sdcardHandler.get());
    auto requestHandler =
        std::make_shared<RequestHandler>("example.com", "80", "token123", wirelessHandler, sdcardHandler);

    std::string request;
    requestHandler->createTimestampGETRequest(&request);

    std::string expected = "GET "
                           "/api/time"
                           " HTTP/1.0\r\n"
                           "Host: example.com:80\r\n"
                           "User-Agent: esp-idf/1.0 esp32\r\n"
                           "Connection: keep-alive\r\n"
                           "\r\n";

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), request.c_str());

    requestHandler.reset();
    wirelessHandler.reset();
    sdcardHandler.reset();
}

void test_createGenericPOSTRequest() {
    auto sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    auto wirelessHandler = std::make_shared<WirelessHandler>(sdcardHandler.get());
    auto requestHandler =
        std::make_shared<RequestHandler>("example.com", "80", "token123", wirelessHandler, sdcardHandler);

    std::string request;
    RequestHandlerReturnCode ret =
        requestHandler->createGenericPOSTRequest(&request, "/api/endpoint", 4, "key1", "value1", "key2", "value2");
    TEST_ASSERT_EQUAL(RequestHandlerReturnCode::SUCCESS, ret);

    std::string content = "{"
                          "key1:value1,"
                          "key2:value2"
                          "}";

    std::string expected = "POST /api/endpoint HTTP/1.0\r\n"
                           "Host: example.com:80\r\n"
                           "User-Agent: esp-idf/1.0 esp32\r\n"
                           "Connection: keep-alive\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: " +
                           std::to_string(content.length()) +
                           "\r\n"
                           "\r\n" +
                           content;

    TEST_ASSERT_EQUAL(RequestHandlerReturnCode::SUCCESS, ret);
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), request.c_str());

    requestHandler.reset();
    wirelessHandler.reset();
    sdcardHandler.reset();
}

void test_parseHttpReturnCode() {
    auto sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    auto wirelessHandler = std::make_shared<WirelessHandler>(sdcardHandler.get());
    auto requestHandler =
        std::make_shared<RequestHandler>("example.com", "80", "token123", wirelessHandler, sdcardHandler);

    const char response1[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 13\r\n\r\nHello, World!";
    const char response2[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 9\r\n\r\nNot Found";
    const char response3[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\nContent-Length: "
                             "30\r\n\r\n{\"error\": \"server malfunction\"}";
    const char response4[] = "HTTP/1.1 301 Moved Permanently\r\nLocation: https://example.com/\r\n\r\n";
    const char response5[] =
        "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: 10\r\n\r\nForbidden";

    TEST_ASSERT_EQUAL(200, requestHandler->parseHttpReturnCode(response1));
    TEST_ASSERT_EQUAL(404, requestHandler->parseHttpReturnCode(response2));
    TEST_ASSERT_EQUAL(500, requestHandler->parseHttpReturnCode(response3));
    TEST_ASSERT_EQUAL(301, requestHandler->parseHttpReturnCode(response4));
    TEST_ASSERT_EQUAL(403, requestHandler->parseHttpReturnCode(response5));

    requestHandler.reset();
    wirelessHandler.reset();
    sdcardHandler.reset();
}

void test_parseResponseIntoJson() {
    auto sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    auto wirelessHandler = std::make_shared<WirelessHandler>(sdcardHandler.get());
    auto requestHandler =
        std::make_shared<RequestHandler>("example.com", "80", "token123", wirelessHandler, sdcardHandler);

    QueueMessage responseBuffer;
    responseBuffer.buffer_length = 0;
    responseBuffer.str_buffer[0] = '\0';

    strncpy(responseBuffer.str_buffer,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 30\r\n\r\n{\"time\": "
            "\"1234567890\"}",
            133);

    responseBuffer.str_buffer[133] = '\0';

    TEST_ASSERT_EQUAL(0, requestHandler->parseResponseIntoJson(&responseBuffer, 133));
    TEST_ASSERT_EQUAL_STRING("{\"time\": \"1234567890\"}", responseBuffer.str_buffer);

    strncpy(responseBuffer.str_buffer,
            "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 9\r\n\r\nNot Found", 81);
    responseBuffer.str_buffer[81] = '\0';
    TEST_ASSERT_EQUAL(1, requestHandler->parseResponseIntoJson(&responseBuffer, 81));

    requestHandler.reset();
    wirelessHandler.reset();
    sdcardHandler.reset();
}

void test_parseTimestamp() {
    auto sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    auto wirelessHandler = std::make_shared<WirelessHandler>(sdcardHandler.get());
    auto requestHandler =
        std::make_shared<RequestHandler>("example.com", "80", "token123", wirelessHandler, sdcardHandler);

    const std::string response1 =
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 30\r\n\r\n1234567890";
    const std::string response2 =
        "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 9\r\n\r\nNot Found";
    const std::string response3 =
        "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\nContent-Length: "
        "30\r\n\r\n{\"error\": \"server malfunction\"}";
    const std::string response4 = "HTTP/1.1 301 Moved Permanently\r\nLocation: https://example.com/\r\n\r\n";
    const std::string response5 =
        "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: 10\r\n\r\nForbidden";

    const std::string response6 = "asdf";

    TEST_ASSERT_EQUAL(1234567890, requestHandler->parseTimestamp(response1));
    TEST_ASSERT_EQUAL(-3, requestHandler->parseTimestamp(response2));
    TEST_ASSERT_EQUAL(-3, requestHandler->parseTimestamp(response3));
    TEST_ASSERT_EQUAL(-2, requestHandler->parseTimestamp(response4));
    TEST_ASSERT_EQUAL(-3, requestHandler->parseTimestamp(response5));
    TEST_ASSERT_EQUAL(-1, requestHandler->parseTimestamp(response6));

    requestHandler.reset();
    wirelessHandler.reset();
    sdcardHandler.reset();
}

void run_all_request_handler_tests() {
    RUN_TEST(test_createImagePOSTRequest);
    RUN_TEST(test_createUserInstructionsGETRequest);
    RUN_TEST(test_createTimestampGETRequest);
    RUN_TEST(test_createGenericPOSTRequest);
    RUN_TEST(test_parseHttpReturnCode);
    RUN_TEST(test_parseResponseIntoJson);
    RUN_TEST(test_parseTimestamp);
}