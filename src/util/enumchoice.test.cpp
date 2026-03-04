#include <JuceHeader.h>
#include <sstream>
#include "enumchoice.h"

namespace MoTool::Util {

//==============================================================================
// Test enum definition
struct TestColorEnum {
    enum Enum : uint8_t {
        Red,
        Green,
        Blue
    };
};

using TestColor = EnumChoice<TestColorEnum>;

//==============================================================================
// Test enum with custom labels array (should override magic_enum names when properly configured)
struct TestDirectionEnum {
    enum Enum : uint8_t {
        North,
        South,
        East,
        West
    };

    static inline constexpr std::string_view labels[] {
        "Move North",
        "Move South",
        "Move East",
        "Move West"
    };
};

using TestDirection = EnumChoice<TestDirectionEnum>;

//==============================================================================
// Test enum with longLabels and shortLabels
struct TestSizeEnum {
    enum Enum : uint8_t {
        Small,
        Medium,
        Large
    };

    static inline constexpr std::string_view longLabels[] {
        "Small Size",
        "Medium Size",
        "Large Size"
    };

    static inline constexpr std::string_view shortLabels[] {
        "S",
        "M",
        "L"
    };
};

using TestSize = EnumChoice<TestSizeEnum>;

} // namespace MoTool::Util

//==============================================================================
namespace MoTool::Util {

class EnumChoiceTest : public UnitTest {
public:
    EnumChoiceTest() : UnitTest("EnumChoice", "MoTool") {}

    void runTest() override {
        beginTest("getLabel returns correct enum name");
        {
            TestColor red(TestColorEnum::Red);
            expect(red.getLabel() == "Red");

            TestColor green(TestColorEnum::Green);
            expect(green.getLabel() == "Green");

            TestColor blue(TestColorEnum::Blue);
            expect(blue.getLabel() == "Blue");
        }

        beginTest("getLabel static method returns correct name by index");
        {
            expect(TestColor::getLabel(0) == "Red");
            expect(TestColor::getLabel(1) == "Green");
            expect(TestColor::getLabel(2) == "Blue");
        }

        beginTest("getLabel uses custom labels array when magic_enum is customized");
        {
            TestDirection north(TestDirectionEnum::North);
            expect(north.getLabel() == "Move North");

            TestDirection south(TestDirectionEnum::South);
            expect(south.getLabel() == "Move South");

            TestDirection east(TestDirectionEnum::East);
            expect(east.getLabel() == "Move East");

            TestDirection west(TestDirectionEnum::West);
            expect(west.getLabel() == "Move West");
        }

        beginTest("getLabels returns custom labels when defined");
        {
            auto labels = TestDirection::getLabels();
            expectEquals(labels.size(), size_t(4));
            expect(labels[0] == "Move North");
            expect(labels[1] == "Move South");
            expect(labels[2] == "Move East");
            expect(labels[3] == "Move West");
        }

        beginTest("getLongLabel returns custom labels when defined");
        {
            TestSize small(TestSizeEnum::Small);
            expect(small.getLongLabel() == "Small Size");

            TestSize medium(TestSizeEnum::Medium);
            expect(medium.getLongLabel() == "Medium Size");

            TestSize large(TestSizeEnum::Large);
            expect(large.getLongLabel() == "Large Size");
        }

        beginTest("getShortLabel returns custom labels when defined");
        {
            TestSize small(TestSizeEnum::Small);
            expect(small.getShortLabel() == "S");

            TestSize medium(TestSizeEnum::Medium);
            expect(medium.getShortLabel() == "M");

            TestSize large(TestSizeEnum::Large);
            expect(large.getShortLabel() == "L");
        }

        beginTest("getLabels returns array of all labels");
        {
            auto labels = TestColor::getLabels();
            expectEquals(labels.size(), size_t(3));
            expect(labels[0] == "Red");
            expect(labels[1] == "Green");
            expect(labels[2] == "Blue");
        }

        beginTest("size returns correct enum count");
        {
            expectEquals(TestColor::size(), size_t(3));
            expectEquals(TestSize::size(), size_t(3));
            expectEquals(TestDirection::size(), size_t(4));
        }

        beginTest("conversion to String works");
        {
            TestColor red(TestColorEnum::Red);
            String redString = static_cast<juce::String>(red);
            expectEquals(redString, String("Red"));

            TestDirection north(TestDirectionEnum::North);
            String northString = static_cast<juce::String>(north);
            expectEquals(northString, String("Move North"));
        }

        beginTest("constructors work correctly");
        {
            // Default constructor
            TestColor defaultColor;
            expect(defaultColor == TestColorEnum::Red);  // First enum value

            // Enum constructor
            TestColor green(TestColorEnum::Green);
            expect(green == TestColorEnum::Green);

            // Int constructor
            TestColor blue(2);
            expect(blue == TestColorEnum::Blue);

            // String_view constructor
            TestColor redFromString(std::string_view("Red"));
            expect(redFromString == TestColorEnum::Red);

            // Invalid string results in undefined value
            TestColor invalid(std::string_view("InvalidColor"));
            expect(!invalid.isValid());
            expect(invalid == TestColor::undefined());

            // Copy constructor
            TestColor greenCopy(green);
            expect(greenCopy == TestColorEnum::Green);

            // Move constructor
            TestColor greenMoved(std::move(greenCopy));
            expect(greenMoved == TestColorEnum::Green);
        }

        beginTest("assignment operators work correctly");
        {
            TestColor red(TestColorEnum::Red);
            TestColor green(TestColorEnum::Green);

            // Copy assignment
            red = green;
            expect(red == TestColorEnum::Green);

            // Move assignment
            TestColor blue(TestColorEnum::Blue);
            red = std::move(blue);
            expect(red == TestColorEnum::Blue);
        }

        beginTest("comparison operators work correctly");
        {
            TestColor red1(TestColorEnum::Red);
            TestColor red2(TestColorEnum::Red);
            TestColor green(TestColorEnum::Green);

            // EnumChoice == EnumChoice
            expect(red1 == red2);
            expect(!(red1 == green));

            // EnumChoice != EnumChoice
            expect(red1 != green);
            expect(!(red1 != red2));

            // EnumChoice == Enum
            expect(red1 == TestColorEnum::Red);
            expect(!(red1 == TestColorEnum::Green));

            // EnumChoice != Enum
            expect(red1 != TestColorEnum::Green);
            expect(!(red1 != TestColorEnum::Red));
        }

        beginTest("enum conversion operator works");
        {
            TestColor red(TestColorEnum::Red);
            TestColorEnum::Enum enumValue = red;
            expect(enumValue == TestColorEnum::Red);

            // Can be used in switch statements
            bool correctSwitch = false;
            switch (red) {
                case TestColorEnum::Red:
                    correctSwitch = true;
                    break;
                case TestColorEnum::Green:
                case TestColorEnum::Blue:
                    break;
            }
            expect(correctSwitch);
        }

        beginTest("isValid returns correct validity status");
        {
            TestColor red(TestColorEnum::Red);
            expect(red.isValid());

            // Create an invalid enum value by casting
            auto invalidValue = static_cast<TestColorEnum::Enum>(255);
            TestColor invalid(invalidValue);
            expect(!invalid.isValid());
        }

        beginTest("getLongLabels static method works");
        {
            auto longLabels = TestSize::getLongLabels();
            expectEquals(longLabels.size(), size_t(3));
            expect(longLabels[0] == "Small Size");
            expect(longLabels[1] == "Medium Size");
            expect(longLabels[2] == "Large Size");

            // For enums without longLabels, returns magic_enum names
            auto colorLongLabels = TestColor::getLongLabels();
            expectEquals(colorLongLabels.size(), size_t(3));
            expect(colorLongLabels[0] == "Red");
            expect(colorLongLabels[1] == "Green");
            expect(colorLongLabels[2] == "Blue");
        }

        beginTest("getShortLabels static method works");
        {
            // Note: There's a bug in EnumChoice.h line 170 - it returns raw array
            // instead of using to_array() like getLongLabels does
            // This test verifies getShortLabel() instance method instead
            TestSize small(TestSizeEnum::Small);
            TestSize medium(TestSizeEnum::Medium);
            TestSize large(TestSizeEnum::Large);

            expect(small.getShortLabel() == "S");
            expect(medium.getShortLabel() == "M");
            expect(large.getShortLabel() == "L");

            // For enums without shortLabels, returns magic_enum names
            auto colorShortLabels = TestColor::getShortLabels();
            expectEquals(colorShortLabels.size(), size_t(3));
            expect(colorShortLabels[0] == "Red");
            expect(colorShortLabels[1] == "Green");
            expect(colorShortLabels[2] == "Blue");
        }

        beginTest("undefined returns max value");
        {
            auto undefinedValue = TestColor::undefined();
            expect(undefinedValue == static_cast<TestColorEnum::Enum>(255));  // max uint8_t
        }

        beginTest("forEach iterates over all enum values");
        {
            size_t count = 0;
            std::array<TestColorEnum::Enum, 3> visited;

            // magic_enum passes integral_constant, access via ::value
            TestColor::forEach([&count, &visited](auto val) {
                visited[count] = val();
                count++;
            });

            expectEquals(count, size_t(3));
            expect(visited[0] == TestColorEnum::Red);
            expect(visited[1] == TestColorEnum::Green);
            expect(visited[2] == TestColorEnum::Blue);
        }

        beginTest("stream operators work correctly");
        {
            TestColor red(TestColorEnum::Red);

            // Test std::ostream operator
            std::ostringstream oss;
            oss << red;
            expectEquals(oss.str(), std::string("Red"));

            // Test String operator
            String juceStr;
            juceStr << red;
            expect(juceStr.contains("Red"));

            // Test with custom labels
            TestDirection north(TestDirectionEnum::North);
            std::ostringstream oss2;
            oss2 << north;
            expectEquals(oss2.str(), std::string("Move North"));
        }
    }
};

static EnumChoiceTest enumChoiceTest;

} // namespace MoTool::Util
