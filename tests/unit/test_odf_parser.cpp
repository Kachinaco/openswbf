// Unit tests for assets/odf/odf_parser.h — ODF (Object Definition File) parser.

#include "assets/odf/odf_parser.h"

#include <gtest/gtest.h>

using namespace swbf;

// ---------------------------------------------------------------------------
// Basic parsing
// ---------------------------------------------------------------------------

TEST(ODFParser, ParsesBasicODF) {
    const std::string odf_text = R"(
[GameObjectClass]
ClassLabel = "soldier"
ClassParent = "rep_inf_default"

[Properties]
GeometryName = "rep_inf_ep3_rifleman"
MaxHealth = 300.0
MaxSpeed = 7
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    // Quick-access convenience fields
    EXPECT_EQ(odf.class_label, "soldier");
    EXPECT_EQ(odf.class_parent, "rep_inf_default");
    EXPECT_EQ(odf.geometry_name, "rep_inf_ep3_rifleman");

    // Section count
    ASSERT_EQ(odf.sections.size(), 2u);
    EXPECT_EQ(odf.sections[0].name, "GameObjectClass");
    EXPECT_EQ(odf.sections[1].name, "Properties");
}

TEST(ODFParser, GeometryNameFromProperties) {
    // When GeometryName is only in [Properties], not [GameObjectClass]
    const std::string odf_text = R"(
[GameObjectClass]
ClassLabel = "vehicle"

[Properties]
GeometryName = "rep_fly_assault_dome"
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.class_label, "vehicle");
    EXPECT_EQ(odf.geometry_name, "rep_fly_assault_dome");
}

TEST(ODFParser, GeometryNameFromGameObjectClass) {
    // When GeometryName is in [GameObjectClass], it takes priority.
    const std::string odf_text = R"(
[GameObjectClass]
ClassLabel = "soldier"
GeometryName = "from_class"

[Properties]
GeometryName = "from_props"
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.geometry_name, "from_class");
}

// ---------------------------------------------------------------------------
// Accessors: get, get_int, get_float
// ---------------------------------------------------------------------------

TEST(ODFParser, GetReturnsValue) {
    const std::string odf_text = R"(
[Properties]
MaxHealth = 300.0
Label = "test_unit"
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.get("Properties", "MaxHealth"), "300.0");
    EXPECT_EQ(odf.get("Properties", "Label"), "test_unit");
}

TEST(ODFParser, GetReturnsDefaultForMissingKey) {
    const std::string odf_text = R"(
[Properties]
MaxHealth = 300.0
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.get("Properties", "MissingKey"), "");
    EXPECT_EQ(odf.get("Properties", "MissingKey", "default"), "default");
}

TEST(ODFParser, GetReturnsDefaultForMissingSection) {
    const std::string odf_text = R"(
[Properties]
MaxHealth = 300.0
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.get("NonExistent", "MaxHealth", "nope"), "nope");
}

TEST(ODFParser, GetInt) {
    const std::string odf_text = R"(
[Properties]
MaxHealth = 300
MaxSpeed = 7
BadInt = notanumber
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.get_int("Properties", "MaxHealth"), 300);
    EXPECT_EQ(odf.get_int("Properties", "MaxSpeed"), 7);
    EXPECT_EQ(odf.get_int("Properties", "BadInt", -1), -1);
    EXPECT_EQ(odf.get_int("Properties", "MissingKey", 42), 42);
}

TEST(ODFParser, GetFloat) {
    const std::string odf_text = R"(
[Properties]
MaxHealth = 300.5
ScaleFactor = 0.75
BadFloat = notafloat
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_FLOAT_EQ(odf.get_float("Properties", "MaxHealth"), 300.5f);
    EXPECT_FLOAT_EQ(odf.get_float("Properties", "ScaleFactor"), 0.75f);
    EXPECT_FLOAT_EQ(odf.get_float("Properties", "BadFloat", -1.0f), -1.0f);
    EXPECT_FLOAT_EQ(odf.get_float("Properties", "MissingKey", 3.14f), 3.14f);
}

// ---------------------------------------------------------------------------
// Comment handling
// ---------------------------------------------------------------------------

TEST(ODFParser, SkipsComments) {
    const std::string odf_text = R"(
// This is a comment
[GameObjectClass]
// Another comment
ClassLabel = "soldier"
// ClassParent = "ignored"
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.class_label, "soldier");
    EXPECT_EQ(odf.class_parent, "");
    ASSERT_EQ(odf.sections.size(), 1u);
    // Only ClassLabel should be present; the commented-out ClassParent should not.
    EXPECT_EQ(odf.sections[0].properties.count("ClassParent"), 0u);
}

// ---------------------------------------------------------------------------
// Quoted values
// ---------------------------------------------------------------------------

TEST(ODFParser, StripsQuotesFromValues) {
    const std::string odf_text = R"(
[Properties]
Name = "quoted_value"
Bare = unquoted_value
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.get("Properties", "Name"), "quoted_value");
    EXPECT_EQ(odf.get("Properties", "Bare"), "unquoted_value");
}

// ---------------------------------------------------------------------------
// Whitespace and formatting robustness
// ---------------------------------------------------------------------------

TEST(ODFParser, HandlesWhitespaceAroundEqualsSign) {
    const std::string odf_text = R"(
[Properties]
Key1=value1
Key2 = value2
Key3   =   value3
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.get("Properties", "Key1"), "value1");
    EXPECT_EQ(odf.get("Properties", "Key2"), "value2");
    EXPECT_EQ(odf.get("Properties", "Key3"), "value3");
}

TEST(ODFParser, SkipsBlankLines) {
    const std::string odf_text = R"(

[GameObjectClass]

ClassLabel = "soldier"

[Properties]

MaxHealth = 100

)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_EQ(odf.class_label, "soldier");
    EXPECT_EQ(odf.get_int("Properties", "MaxHealth"), 100);
}

// ---------------------------------------------------------------------------
// Empty and malformed input
// ---------------------------------------------------------------------------

TEST(ODFParser, EmptyString) {
    ODFParser parser;
    ODFFile odf = parser.parse("");

    EXPECT_EQ(odf.class_label, "");
    EXPECT_EQ(odf.class_parent, "");
    EXPECT_EQ(odf.geometry_name, "");
    EXPECT_TRUE(odf.sections.empty());
}

TEST(ODFParser, NoSections) {
    // Key-value pairs before any section header should be ignored.
    const std::string odf_text = R"(
Key = Value
AnotherKey = AnotherValue
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    EXPECT_TRUE(odf.sections.empty());
}

TEST(ODFParser, MalformedSectionHeader) {
    // Missing closing bracket should be skipped (treated as malformed).
    const std::string odf_text = R"(
[MissClosingBracket
Key = Value

[Valid]
Key2 = Value2
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    // Only [Valid] should be parsed as a section.
    ASSERT_EQ(odf.sections.size(), 1u);
    EXPECT_EQ(odf.sections[0].name, "Valid");
    EXPECT_EQ(odf.get("Valid", "Key2"), "Value2");
}

TEST(ODFParser, MultipleSections) {
    const std::string odf_text = R"(
[GameObjectClass]
ClassLabel = "hover"
ClassParent = "rep_hover_default"

[Properties]
MaxHealth = 500
GeometryName = "rep_hover_aat"

[WeaponSection]
WeaponName = "rep_weap_aat_cannon"
FireRate = 2.0
)";

    ODFParser parser;
    ODFFile odf = parser.parse(odf_text);

    ASSERT_EQ(odf.sections.size(), 3u);
    EXPECT_EQ(odf.class_label, "hover");
    EXPECT_EQ(odf.class_parent, "rep_hover_default");
    EXPECT_EQ(odf.geometry_name, "rep_hover_aat");
    EXPECT_EQ(odf.get("WeaponSection", "WeaponName"), "rep_weap_aat_cannon");
    EXPECT_FLOAT_EQ(odf.get_float("WeaponSection", "FireRate"), 2.0f);
}

// ---------------------------------------------------------------------------
// Parse from byte vector
// ---------------------------------------------------------------------------

TEST(ODFParser, ParseFromByteVector) {
    const std::string text = R"(
[GameObjectClass]
ClassLabel = "soldier"
)";

    std::vector<uint8_t> data(text.begin(), text.end());

    ODFParser parser;
    ODFFile odf = parser.parse(data);

    EXPECT_EQ(odf.class_label, "soldier");
}
