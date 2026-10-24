#pragma once

#include <QFont>
#include <QStringList>

namespace ThemeFonts
{
inline QStringList families()
{
    return {QStringLiteral("Times New Roman"), QStringLiteral("Liberation Serif"),
            QStringLiteral("Noto Serif CJK SC"), QStringLiteral("serif")};
}

inline QFont text(int pixelSize = 13, QFont::Weight weight = QFont::Normal)
{
    QFont font;
    font.setFamilies(families());
    font.setStyleHint(QFont::Serif);
    font.setPixelSize(qMax(8, pixelSize));
    font.setWeight(weight);
    return font;
}
}
