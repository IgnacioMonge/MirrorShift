#ifndef INPUT_HELPERS_H
#define INPUT_HELPERS_H

#include <QString>

namespace InputHelpers {

QString squareName(int row, int col);

bool isMoveSyntaxOk(const QString &move);
bool isMqttRoomSyntaxOk(const QString &room);
bool isDirectIpSyntaxOk(const QString &host);

} // namespace InputHelpers

#endif // INPUT_HELPERS_H
