#ifndef PIECE_RENDERER_H
#define PIECE_RENDERER_H

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QString>
#include <QStringList>

namespace PieceRenderer {

QString assetPath(const QString &relativePath);

void setBoardTexture(const QString &name);
QString boardTexture();
QStringList glassBoardThemeIds();
QString glassBoardThemeTitle(const QString &id);
bool isGlassBoardTheme(const QString &id);
QColor glassBoardFrameColor(const QString &id);
QColor boardWellColor(int squareSize);

struct GlassMeterPalette {
    QColor selfFar;
    QColor selfMid;
    QColor selfNear;
    QColor echoFar;
    QColor echoMid;
    QColor echoNear;
    QColor seam;
    QColor well;
};
QString boardSquareStyle(int row, int col, int squareSize,
                         const QString &background, const QString &foreground,
                         const QString &border);
QIcon boardSquareIcon(char piece, int row, int col, int squareSize,
                      int pieceIconSize, bool pieceVisible, bool legalHint,
                      bool targetHighlight);
QIcon boardSquareIconFlip(char startFace, char endFace, int row, int col,
                          int squareSize, int pieceIconSize, qreal angle);

QIcon pieceIcon(char piece);
void prewarmPieceIcons();

QIcon makeMirrorShiftIcon();

QImage boardTextureImage(int squareSize);

} // namespace PieceRenderer

#endif // PIECE_RENDERER_H
