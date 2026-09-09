#include "pc/client/input_helpers.h"

#include <cstdio>

static int failures;

static void check(bool ok, const char *label)
{
    if (!ok) {
        std::printf("FAIL: %s\n", label);
        ++failures;
    }
}

int main()
{
    check(InputHelpers::squareName(7, 4) == QStringLiteral("e1"),
          "square name");
    check(InputHelpers::isMoveSyntaxOk(QStringLiteral("d3")),
          "Lattice square syntax");
    check(!InputHelpers::isMoveSyntaxOk(QStringLiteral("e2e4")),
          "inherited chess syntax rejected");
    check(InputHelpers::isMqttRoomSyntaxOk(QStringLiteral("MS12AF")),
          "six-character room syntax");
    check(!InputHelpers::isMqttRoomSyntaxOk(QStringLiteral("MS12A")),
          "short room syntax");
    check(!InputHelpers::isMqttRoomSyntaxOk(QStringLiteral("MS12AF0")),
          "overlong room syntax");
    check(!InputHelpers::isMqttRoomSyntaxOk(QStringLiteral("MS12AG")),
          "non-hexadecimal room syntax");
    check(!InputHelpers::isMqttRoomSyntaxOk(QStringLiteral("AB12AF")),
          "room prefix syntax");
    check(!InputHelpers::isMqttRoomSyntaxOk(QStringLiteral("ms12af")),
          "lowercase room syntax");
    check(InputHelpers::isDirectIpSyntaxOk(QStringLiteral("127.0.0.1")),
          "IPv4 syntax");
    check(!InputHelpers::isDirectIpSyntaxOk(QStringLiteral("localhost")),
          "hostname is not direct IPv4");

    if (failures != 0) {
        return 1;
    }
    std::printf("input helper tests ok\n");
    return 0;
}
