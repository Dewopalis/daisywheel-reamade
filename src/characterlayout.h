#pragma once

#include <array>
#include <QByteArray>
#include <QString>

namespace characterlayout {
using Grid = std::array<std::array<QString, 4>, 8>;
using Pages = std::array<Grid, 3>;

struct Layout {
    QString name;
    Pages lower;
    Pages upper;
};

Layout builtIn();
bool parseText(const QByteArray &contents, Layout &layout, QString &error);
QByteArray toText(const Layout &layout);
}
