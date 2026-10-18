#include "converter.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

using dataconv::convert_text;
using json = nlohmann::json;

TEST(Converter, CsvToJson) {
    const auto result = json::parse(convert_text("name,age\nEva,36\nLinus,55\n", "csv", "json"));
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0]["name"], "Eva");
    EXPECT_EQ(result[1]["age"], "55"); // CSV values are strings by contract.
}

TEST(Converter, CsvQuotedComma) {
    const auto result = json::parse(convert_text("name,note\nEva,\"hello, world\"\n", "csv", "json"));
    EXPECT_EQ(result[0]["note"], "hello, world");
}

TEST(Converter, CsvEscapedQuote) {
    const auto result = json::parse(convert_text("name,note\nEva,\"said \"\"hi\"\"\"\n", "csv", "json"));
    EXPECT_EQ(result[0]["note"], "said \"hi\"");
}

TEST(Converter, JsonToCsv) {
    const auto result = convert_text(R"([{"name":"Eva","city":"Moscow"}])", "json", "csv");
    EXPECT_NE(result.find("name"), std::string::npos);
    EXPECT_NE(result.find("Eva"), std::string::npos);
}

TEST(Converter, JsonToYaml) {
    const auto result = convert_text(R"([{"active":true,"name":"Eva"}])", "json", "yaml");
    EXPECT_NE(result.find("Eva"), std::string::npos);
}

TEST(Converter, YamlToJson) {
    const auto result = json::parse(convert_text("- name: Eva\n  age: 36\n", "yaml", "json"));
    EXPECT_EQ(result[0]["name"], "Eva");
    EXPECT_EQ(result[0]["age"], 36);
}

TEST(Converter, YmlExtensionAccepted) {
    const auto result = json::parse(convert_text("- name: Eva\n", "yml", "json"));
    EXPECT_EQ(result[0]["name"], "Eva");
}

TEST(Converter, CsvWrongColumnCount) {
    EXPECT_THROW(convert_text("a,b\n1\n", "csv", "json"), std::runtime_error);
}

TEST(Converter, InvalidExtension) {
    EXPECT_THROW(convert_text("x", "xml", "json"), std::runtime_error);
}

TEST(Converter, SameFormatRejected) {
    EXPECT_THROW(convert_text("[]", "json", "json"), std::runtime_error);
}

TEST(Converter, NestedObjectCannotExportToCsv) {
    EXPECT_THROW(convert_text(R"([{"name":"Eva","meta":{"id":1}}])", "json", "csv"), std::runtime_error);
}

TEST(Converter, MalformedJsonRejected) {
    EXPECT_THROW(convert_text("[", "json", "yaml"), std::exception);
}

TEST(Converter, EmptyCsvProducesEmptyArray) {
    const auto result = json::parse(convert_text("", "csv", "json"));
    EXPECT_TRUE(result.is_array());
    EXPECT_TRUE(result.empty());
}
