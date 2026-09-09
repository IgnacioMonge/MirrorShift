#include "input_helpers.h"

#include <QHostAddress>
#include <QNetworkInterface>

namespace InputHelpers {

static bool isMoveFile(QChar ch)
{
    return ch >= QLatin1Char('a') && ch <= QLatin1Char('h');
}

static bool isMoveRank(QChar ch)
{
    return ch >= QLatin1Char('1') && ch <= QLatin1Char('8');
}

QString squareName(int row, int col)
{
    const QChar file('a' + col);
    const QChar rank('8' - row);
    return QString(file) + QString(rank);
}

bool isMoveSyntaxOk(const QString &move)
{
    return move.size() == 2 && isMoveFile(move.at(0)) &&
           isMoveRank(move.at(1));
}

bool isMqttRoomSyntaxOk(const QString &room)
{
    if (room.size() != 6 || !room.startsWith(QStringLiteral("MS"))) return false;
    for (const QChar ch : room.sliced(2)) {
        const ushort c = ch.unicode();
        if ((c >= 'A' && c <= 'F') || (c >= '0' && c <= '9')) continue;
        return false;
    }
    return true;
}

bool isDirectIpSyntaxOk(const QString &host)
{
    QHostAddress address;
    return address.setAddress(host) &&
           address.protocol() == QAbstractSocket::IPv4Protocol &&
           !address.isNull();
}

} // namespace InputHelpers
