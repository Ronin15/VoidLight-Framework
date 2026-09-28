#define BOOST_TEST_MODULE JsonReaderTest
#include "utils/JsonReader.hpp"
#include <boost/test/unit_test.hpp>
#include <fstream>

using namespace VoidLight;

BOOST_AUTO_TEST_SUITE(JsonValueTests)

BOOST_AUTO_TEST_CASE(TestBasicTypes) {
  // Null
    JsonValue nullVal;
    BOOST_CHECK(nullVal.isNull());
    BOOST_CHECK_EQUAL(nullVal.getType(), JsonType::Null);
    BOOST_CHECK_EQUAL(nullVal.toString(), "null");

  // Boolean
    JsonValue trueVal(true);
    JsonValue falseVal(false);
    BOOST_CHECK(trueVal.isBool());
    BOOST_CHECK(falseVal.isBool());
    BOOST_CHECK_EQUAL(trueVal.asBool(), true);
    BOOST_CHECK_EQUAL(falseVal.asBool(), false);
    BOOST_CHECK_EQUAL(trueVal.toString(), "true");
    BOOST_CHECK_EQUAL(falseVal.toString(), "false");

  // Number
    JsonValue intVal(42);
    JsonValue doubleVal(3.14);
    BOOST_CHECK(intVal.isNumber());
    BOOST_CHECK(doubleVal.isNumber());
    BOOST_CHECK_EQUAL(intVal.asInt(), 42);
    BOOST_CHECK_CLOSE(doubleVal.asNumber(), 3.14, 0.001);

  // String
    JsonValue stringVal("hello");
    BOOST_CHECK(stringVal.isString());
    BOOST_CHECK_EQUAL(stringVal.asString(), "hello");
    BOOST_CHECK_EQUAL(stringVal.toString(), "\"hello\"");
}

BOOST_AUTO_TEST_CASE(TestArrayOperations) {
    JsonArray arr;
    arr.push_back(JsonValue(1));
    arr.push_back(JsonValue("test"));
    arr.push_back(JsonValue(true));

    JsonValue arrayVal(arr);
    BOOST_CHECK(arrayVal.isArray());
    BOOST_CHECK_EQUAL(arrayVal.size(), 3);
    BOOST_CHECK_EQUAL(arrayVal[0].asInt(), 1);
    BOOST_CHECK_EQUAL(arrayVal[1].asString(), "test");
    BOOST_CHECK_EQUAL(arrayVal[2].asBool(), true);
}

BOOST_AUTO_TEST_CASE(TestObjectOperations) {
    JsonObject obj;
    obj["name"] = JsonValue("John");
    obj["age"] = JsonValue(30);
    obj["active"] = JsonValue(true);

    JsonValue objectVal(obj);
    BOOST_CHECK(objectVal.isObject());
    BOOST_CHECK_EQUAL(objectVal.size(), 3);
    BOOST_CHECK(objectVal.hasKey("name"));
    BOOST_CHECK(objectVal.hasKey("age"));
    BOOST_CHECK(!objectVal.hasKey("missing"));
    BOOST_CHECK_EQUAL(objectVal["name"].asString(), "John");
    BOOST_CHECK_EQUAL(objectVal["age"].asInt(), 30);
    BOOST_CHECK_EQUAL(objectVal["active"].asBool(), true);
}

BOOST_AUTO_TEST_CASE(TestSafeAccessors) {
    JsonValue stringVal("test");
    JsonValue numberVal(42);

  // Valid conversions
    BOOST_CHECK(stringVal.tryAsString().has_value());
    BOOST_CHECK_EQUAL(stringVal.tryAsString().value(), "test");
    BOOST_CHECK(numberVal.tryAsInt().has_value());
    BOOST_CHECK_EQUAL(numberVal.tryAsInt().value(), 42);

  // Invalid conversions
    BOOST_CHECK(!stringVal.tryAsInt().has_value());
    BOOST_CHECK(!numberVal.tryAsString().has_value());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(JsonReaderParsingTests)

BOOST_AUTO_TEST_CASE(TestBasicParsing) {
    JsonReader reader;

  // Null
    BOOST_CHECK(reader.parse("null"));
    BOOST_CHECK(reader.getRoot().isNull());

  // Boolean
    BOOST_CHECK(reader.parse("true"));
    BOOST_CHECK_EQUAL(reader.getRoot().asBool(), true);

    BOOST_CHECK(reader.parse("false"));
    BOOST_CHECK_EQUAL(reader.getRoot().asBool(), false);

  // Number
    BOOST_CHECK(reader.parse("42"));
    BOOST_CHECK_EQUAL(reader.getRoot().asInt(), 42);

    BOOST_CHECK(reader.parse("3.14"));
    BOOST_CHECK_CLOSE(reader.getRoot().asNumber(), 3.14, 0.001);

    BOOST_CHECK(reader.parse("-123"));
    BOOST_CHECK_EQUAL(reader.getRoot().asInt(), -123);

    BOOST_CHECK(reader.parse("1.5e2"));
    BOOST_CHECK_CLOSE(reader.getRoot().asNumber(), 150.0, 0.001);

  // String
    BOOST_CHECK(reader.parse("\"hello\""));
    BOOST_CHECK_EQUAL(reader.getRoot().asString(), "hello");
}

BOOST_AUTO_TEST_CASE(TestStringEscapes) {
    JsonReader reader;

  // Basic escapes
    BOOST_CHECK(reader.parse("\"hello\\nworld\""));
    BOOST_CHECK_EQUAL(reader.getRoot().asString(), "hello\nworld");

    BOOST_CHECK(reader.parse("\"tab\\there\""));
    BOOST_CHECK_EQUAL(reader.getRoot().asString(), "tab\there");

    BOOST_CHECK(reader.parse("\"quote\\\"here\""));
    BOOST_CHECK_EQUAL(reader.getRoot().asString(), "quote\"here");

    BOOST_CHECK(reader.parse("\"backslash\\\\here\""));
    BOOST_CHECK_EQUAL(reader.getRoot().asString(), "backslash\\here");

  // Unicode escape
    BOOST_CHECK(reader.parse("\"\\u0041\""));
    BOOST_CHECK_EQUAL(reader.getRoot().asString(), "A");
}

BOOST_AUTO_TEST_CASE(TestArrayParsing) {
    JsonReader reader;

  // Empty array
    BOOST_CHECK(reader.parse("[]"));
    BOOST_CHECK(reader.getRoot().isArray());
    BOOST_CHECK_EQUAL(reader.getRoot().size(), 0);

  // Simple array
    BOOST_CHECK(reader.parse("[1, 2, 3]"));
    const auto& arr = reader.getRoot();
    BOOST_CHECK(arr.isArray());
    BOOST_CHECK_EQUAL(arr.size(), 3);
    BOOST_CHECK_EQUAL(arr[0].asInt(), 1);
    BOOST_CHECK_EQUAL(arr[1].asInt(), 2);
    BOOST_CHECK_EQUAL(arr[2].asInt(), 3);

  // Mixed types
    BOOST_CHECK(reader.parse("[1, \"hello\", true, null]"));
    const auto& mixedArr = reader.getRoot();
    BOOST_CHECK_EQUAL(mixedArr.size(), 4);
    BOOST_CHECK_EQUAL(mixedArr[0].asInt(), 1);
    BOOST_CHECK_EQUAL(mixedArr[1].asString(), "hello");
    BOOST_CHECK_EQUAL(mixedArr[2].asBool(), true);
    BOOST_CHECK(mixedArr[3].isNull());
}

BOOST_AUTO_TEST_CASE(TestObjectParsing) {
    JsonReader reader;

  // Empty object
    BOOST_CHECK(reader.parse("{}"));
    BOOST_CHECK(reader.getRoot().isObject());
    BOOST_CHECK_EQUAL(reader.getRoot().size(), 0);

  // Simple object
    BOOST_CHECK(reader.parse("{\"name\": \"John\", \"age\": 30}"));
    const auto& obj = reader.getRoot();
    BOOST_CHECK(obj.isObject());
    BOOST_CHECK_EQUAL(obj.size(), 2);
    BOOST_CHECK_EQUAL(obj["name"].asString(), "John");
    BOOST_CHECK_EQUAL(obj["age"].asInt(), 30);
}

BOOST_AUTO_TEST_CASE(TestNestedStructures) {
    JsonReader reader;

    std::string complexJson = R"({
        "person": {
            "name": "Alice",
            "age": 25,
            "hobbies": ["reading", "gaming"],
            "address": {
                "city": "New York",
                "zip": 10001
            }
        },
        "active": true,
        "scores": [85, 92, 78]
    })";

    BOOST_CHECK(reader.parse(complexJson));
    const auto& root = reader.getRoot();

    BOOST_CHECK(root.isObject());
    BOOST_CHECK(root.hasKey("person"));

    const auto& person = root["person"];
    BOOST_CHECK_EQUAL(person["name"].asString(), "Alice");
    BOOST_CHECK_EQUAL(person["age"].asInt(), 25);

    const auto& hobbies = person["hobbies"];
    BOOST_CHECK(hobbies.isArray());
    BOOST_CHECK_EQUAL(hobbies.size(), 2);
    BOOST_CHECK_EQUAL(hobbies[0].asString(), "reading");
    BOOST_CHECK_EQUAL(hobbies[1].asString(), "gaming");

    const auto& address = person["address"];
    BOOST_CHECK_EQUAL(address["city"].asString(), "New York");
    BOOST_CHECK_EQUAL(address["zip"].asInt(), 10001);

    BOOST_CHECK_EQUAL(root["active"].asBool(), true);

    const auto& scores = root["scores"];
    BOOST_CHECK_EQUAL(scores.size(), 3);
    BOOST_CHECK_EQUAL(scores[0].asInt(), 85);
    BOOST_CHECK_EQUAL(scores[1].asInt(), 92);
    BOOST_CHECK_EQUAL(scores[2].asInt(), 78);
}

BOOST_AUTO_TEST_CASE(TestWhitespace) {
    JsonReader reader;

  // Various whitespace combinations
    BOOST_CHECK(reader.parse("  \t\n  42  \r\n  "));
    BOOST_CHECK_EQUAL(reader.getRoot().asInt(), 42);

    BOOST_CHECK(reader.parse("[\n  1,\n  2,\n  3\n]"));
    const auto& arr = reader.getRoot();
    BOOST_CHECK_EQUAL(arr.size(), 3);
    BOOST_CHECK_EQUAL(arr[0].asInt(), 1);
    BOOST_CHECK_EQUAL(arr[1].asInt(), 2);
    BOOST_CHECK_EQUAL(arr[2].asInt(), 3);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(JsonReaderErrorTests)

BOOST_AUTO_TEST_CASE(TestInvalidJSON) {
    JsonReader reader;

  // Missing quotes
    BOOST_CHECK(!reader.parse("hello"));
    BOOST_CHECK(!reader.getLastError().empty());

  // Trailing comma in object
    BOOST_CHECK(!reader.parse("{\"key\": \"value\",}"));
    BOOST_CHECK(!reader.getLastError().empty());

  // Trailing comma in array
    BOOST_CHECK(!reader.parse("[1, 2, 3,]"));
    BOOST_CHECK(!reader.getLastError().empty());

  // Missing closing brace
    BOOST_CHECK(!reader.parse("{\"key\": \"value\""));
    BOOST_CHECK(!reader.getLastError().empty());

  // Missing closing bracket
    BOOST_CHECK(!reader.parse("[1, 2, 3"));
    BOOST_CHECK(!reader.getLastError().empty());

  // Invalid number
    BOOST_CHECK(!reader.parse("123."));
    BOOST_CHECK(!reader.getLastError().empty());

  // Unterminated string
    BOOST_CHECK(!reader.parse("\"hello"));
    BOOST_CHECK(!reader.getLastError().empty());

  // Invalid escape sequence
    BOOST_CHECK(!reader.parse("\"hello\\x\""));
    BOOST_CHECK(!reader.getLastError().empty());

  // Multiple root values
    BOOST_CHECK(!reader.parse("42 43"));
    BOOST_CHECK(!reader.getLastError().empty());
}

BOOST_AUTO_TEST_CASE(TestMalformedStructures) {
    JsonReader reader;

  // Missing colon in object
    BOOST_CHECK(!reader.parse("{\"key\" \"value\"}"));
    BOOST_CHECK(!reader.getLastError().empty());

  // Non-string key in object
    BOOST_CHECK(!reader.parse("{42: \"value\"}"));
    BOOST_CHECK(!reader.getLastError().empty());

  // Missing comma between object members
    BOOST_CHECK(!reader.parse("{\"key1\": \"value1\" \"key2\": \"value2\"}"));
    BOOST_CHECK(!reader.getLastError().empty());

  // Missing comma between array elements
    BOOST_CHECK(!reader.parse("[1 2 3]"));
    BOOST_CHECK(!reader.getLastError().empty());
}

BOOST_AUTO_TEST_CASE(TestInvalidTokens) {
    JsonReader reader;

  // Invalid literal
    BOOST_CHECK(!reader.parse("truee"));
    BOOST_CHECK(!reader.getLastError().empty());

    BOOST_CHECK(!reader.parse("falsee"));
    BOOST_CHECK(!reader.getLastError().empty());

    BOOST_CHECK(!reader.parse("nulll"));
    BOOST_CHECK(!reader.getLastError().empty());

  // Invalid characters
    BOOST_CHECK(!reader.parse("@"));
    BOOST_CHECK(!reader.getLastError().empty());

    BOOST_CHECK(!reader.parse("#"));
    BOOST_CHECK(!reader.getLastError().empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(JsonReaderFileTests)

BOOST_AUTO_TEST_CASE(TestFileLoading) {
  // Create a temporary JSON file
    std::string filename = "test_temp.json";
    std::string jsonContent = R"({
        "name": "Test Item",
        "type": "weapon",
        "stats": {
            "damage": 50,
            "accuracy": 0.85
        },
        "tags": ["rare", "magical"]
    })";

    {
        std::ofstream file(filename);
        file << jsonContent;
    }

    JsonReader reader;
    BOOST_CHECK(reader.loadFromFile(filename));

    const auto& root = reader.getRoot();
    BOOST_CHECK_EQUAL(root["name"].asString(), "Test Item");
    BOOST_CHECK_EQUAL(root["type"].asString(), "weapon");
    BOOST_CHECK_EQUAL(root["stats"]["damage"].asInt(), 50);
    BOOST_CHECK_CLOSE(root["stats"]["accuracy"].asNumber(), 0.85, 0.001);

    const auto& tags = root["tags"];
    BOOST_CHECK_EQUAL(tags.size(), 2);
    BOOST_CHECK_EQUAL(tags[0].asString(), "rare");
    BOOST_CHECK_EQUAL(tags[1].asString(), "magical");

  // Clean up
    std::remove(filename.c_str());
}

BOOST_AUTO_TEST_CASE(TestNonExistentFile) {
    JsonReader reader;
    BOOST_CHECK(!reader.loadFromFile("non_existent_file.json"));
    BOOST_CHECK(!reader.getLastError().empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(JsonReaderItemExampleTests)

BOOST_AUTO_TEST_CASE(TestGameItemsJSON) {
    JsonReader reader;

    std::string itemsJson = R"({
        "items": [
            {
                "id": "sword_001",
                "name": "Iron Sword",
                "type": "weapon",
                "rarity": "common",
                "stats": {
                    "damage": 25,
                    "durability": 100,
                    "weight": 3.5
                },
                "requirements": {
                    "level": 5,
                    "strength": 10
                },
                "effects": [
                    {
                        "type": "damage_bonus",
                        "value": 5,
                        "condition": "critical_hit"
                    }
                ],
                "description": "A sturdy iron sword suitable for beginning adventurers.",
                "stackable": false,
                "value": 150
            },
            {
                "id": "potion_001",
                "name": "Health Potion",
                "type": "consumable",
                "rarity": "common",
                "stats": {
                    "healing": 50,
                    "weight": 0.2
                },
                "effects": [
                    {
                        "type": "heal",
                        "value": 50,
                        "duration": 0
                    }
                ],
                "description": "Restores 50 health points when consumed.",
                "stackable": true,
                "max_stack": 99,
                "value": 25
            }
        ],
        "metadata": {
            "version": "1.0",
            "total_items": 2,
            "last_updated": "2025-01-24"
        }
    })";

    BOOST_CHECK(reader.parse(itemsJson));
    const auto& root = reader.getRoot();

  // Check metadata
    const auto& metadata = root["metadata"];
    BOOST_CHECK_EQUAL(metadata["version"].asString(), "1.0");
    BOOST_CHECK_EQUAL(metadata["total_items"].asInt(), 2);

  // Check items array
    const auto& items = root["items"];
    BOOST_CHECK(items.isArray());
    BOOST_CHECK_EQUAL(items.size(), 2);

  // Check first item (sword)
    const auto& sword = items[0];
    BOOST_CHECK_EQUAL(sword["id"].asString(), "sword_001");
    BOOST_CHECK_EQUAL(sword["name"].asString(), "Iron Sword");
    BOOST_CHECK_EQUAL(sword["type"].asString(), "weapon");
    BOOST_CHECK_EQUAL(sword["rarity"].asString(), "common");
    BOOST_CHECK_EQUAL(sword["stackable"].asBool(), false);
    BOOST_CHECK_EQUAL(sword["value"].asInt(), 150);

    const auto& swordStats = sword["stats"];
    BOOST_CHECK_EQUAL(swordStats["damage"].asInt(), 25);
    BOOST_CHECK_EQUAL(swordStats["durability"].asInt(), 100);
    BOOST_CHECK_CLOSE(swordStats["weight"].asNumber(), 3.5, 0.001);

    const auto& requirements = sword["requirements"];
    BOOST_CHECK_EQUAL(requirements["level"].asInt(), 5);
    BOOST_CHECK_EQUAL(requirements["strength"].asInt(), 10);

    const auto& swordEffects = sword["effects"];
    BOOST_CHECK_EQUAL(swordEffects.size(), 1);
    BOOST_CHECK_EQUAL(swordEffects[0]["type"].asString(), "damage_bonus");
    BOOST_CHECK_EQUAL(swordEffects[0]["value"].asInt(), 5);

  // Check second item (potion)
    const auto& potion = items[1];
    BOOST_CHECK_EQUAL(potion["id"].asString(), "potion_001");
    BOOST_CHECK_EQUAL(potion["name"].asString(), "Health Potion");
    BOOST_CHECK_EQUAL(potion["type"].asString(), "consumable");
    BOOST_CHECK_EQUAL(potion["stackable"].asBool(), true);
    BOOST_CHECK_EQUAL(potion["max_stack"].asInt(), 99);

    const auto& potionStats = potion["stats"];
    BOOST_CHECK_EQUAL(potionStats["healing"].asInt(), 50);
    BOOST_CHECK_CLOSE(potionStats["weight"].asNumber(), 0.2, 0.001);
}

BOOST_AUTO_TEST_SUITE_END()
