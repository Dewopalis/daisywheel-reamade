#include "characterlayout.h"
#include "wheel.h"

#include <QRegularExpression>
#include <QStringList>
#include <utility>

namespace characterlayout {
namespace {
const QStringList sectionNames={
    "letters.lower", "letters.upper", "symbols", "extended.lower", "extended.upper"
};
const QStringList petalNames={
    "North", "Northeast", "East", "Southeast", "South", "Southwest", "West", "Northwest"
};

bool parseSectionRow(const QString &line, int section, Pages &lower, Pages &upper,
                     std::array<std::array<bool, 8>, 5> &seen, int lineNumber,
                     QString &error) {
    const int equals=line.indexOf('=');
    if(equals<0){error=QString("Line %1 needs an '=' after the petal name.").arg(lineNumber);return false;}
    const QString petalName=line.left(equals).trimmed();
    const int petal=petalNames.indexOf(petalName,0,Qt::CaseInsensitive);
    if(petal<0){error=QString("Line %1 has an unknown petal name.").arg(lineNumber);return false;}
    if(seen[section][petal]){error=QString("Line %1 repeats the %2 petal in this section.").arg(lineNumber).arg(petalName);return false;}

    const QStringList characters=line.mid(equals+1).trimmed().split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
    if(characters.size()!=4){error=QString("Line %1 needs four characters in A, B, X, Y order.").arg(lineNumber);return false;}
    for(int slot=0;slot<4;++slot){
        if(characters[slot].toUcs4().size()!=1){error=QString("Line %1: each slot must be one Unicode character.").arg(lineNumber);return false;}
        switch(section){
        case 0:lower[0][petal][slot]=characters[slot];break;
        case 1:upper[0][petal][slot]=characters[slot];break;
        case 2:lower[1][petal][slot]=upper[1][petal][slot]=characters[slot];break;
        case 3:lower[2][petal][slot]=characters[slot];break;
        case 4:upper[2][petal][slot]=characters[slot];break;
        }
    }
    seen[section][petal]=true;
    return true;
}

QString sectionText(const QString &title,const Grid &grid) {
    QString output="["+title+"]\n";
    for(int petal=0;petal<8;++petal){
        output+=petalNames[petal]+" =";
        for(int slot=0;slot<4;++slot)output+=" "+grid[petal][slot];
        output+='\n';
    }
    output+='\n';
    return output;
}
}

Layout builtIn() {
    Layout layout;
    layout.name = "Extended special characters";
    for (int petal = 0; petal < 8; ++petal) {
        for (int slot = 0; slot < 4; ++slot) {
            layout.lower[0][petal][slot] = QString(QChar::fromLatin1(wheel::letters[petal][slot]));
            layout.upper[0][petal][slot] = layout.lower[0][petal][slot].toUpper();
            layout.lower[1][petal][slot] = QString(QChar::fromLatin1(wheel::symbols[petal][slot]));
            layout.upper[1][petal][slot] = layout.lower[1][petal][slot];
            layout.lower[2][petal][slot] = QString::fromUtf8(wheel::extendedLower[petal][slot]);
            layout.upper[2][petal][slot] = QString::fromUtf8(wheel::extendedUpper[petal][slot]);
        }
    }
    return layout;
}

bool parseText(const QByteArray &contents, Layout &layout, QString &error) {
    constexpr qsizetype maximumFileSize=64*1024;
    if(contents.size()>maximumFileSize){error="Layout files must be 64 KB or smaller.";return false;}
    QString text=QString::fromUtf8(contents);
    if(text.startsWith(QChar::ByteOrderMark))text.remove(0,1);
    if(text.toUtf8()!=contents && !(contents.startsWith("\xEF\xBB\xBF") && text.toUtf8()==contents.mid(3))){
        error="The layout file must be valid UTF-8 text.";
        return false;
    }

    QString name;
    int activeSection=-1;
    int lineNumber=0;
    bool headerSeen=false;
    Layout parsed;
    std::array<bool,5> sectionSeen{};
    std::array<std::array<bool,8>,5> seen{};
    QStringList lines=text.split('\n');
    for(QString line:lines){
        ++lineNumber;
        line=line.trimmed();
        if(line.isEmpty()||line.startsWith('#'))continue;
        if(!headerSeen){
            if(line!="DAISYWHEEL LAYOUT V1"){error="The first line must be 'DAISYWHEEL LAYOUT V1'.";return false;}
            headerSeen=true;
            continue;
        }
        if(line.startsWith('[')&&line.endsWith(']')){
            const QString section=line.mid(1,line.size()-2).trimmed().toLower();
            activeSection=sectionNames.indexOf(section);
            if(activeSection<0){error=QString("Line %1 has an unknown section.").arg(lineNumber);return false;}
            if(sectionSeen[activeSection]){error=QString("Line %1 repeats section [%2].").arg(lineNumber).arg(section);return false;}
            sectionSeen[activeSection]=true;
            continue;
        }
        if(activeSection<0&&line.contains('=')){
            const int equals=line.indexOf('=');
            if(line.left(equals).trimmed().compare("Name",Qt::CaseInsensitive)!=0||!name.isEmpty()){
                error=QString("Line %1 has an invalid or duplicate Name field.").arg(lineNumber);return false;
            }
            name=line.mid(equals+1).trimmed();
            if(name.isEmpty()||name.size()>64){error="Name must contain 1 to 64 characters.";return false;}
            parsed.name=name;
            continue;
        }
        if(activeSection<0){error=QString("Line %1 must be a section, a Name field, or a comment.").arg(lineNumber);return false;}
        if(!parseSectionRow(line,activeSection,parsed.lower,parsed.upper,seen,lineNumber,error))return false;
    }
    if(!headerSeen||name.isEmpty()){error="The file needs a header and a Name field.";return false;}
    for(int section=0;section<5;++section){
        if(!sectionSeen[section]){error=QString("Missing section [%1].").arg(sectionNames[section]);return false;}
        for(int petal=0;petal<8;++petal){
            if(!seen[section][petal]){error=QString("Section [%1] is missing the %2 petal.").arg(sectionNames[section],petalNames[petal]);return false;}
        }
    }
    layout=std::move(parsed);
    return true;
}

QByteArray toText(const Layout &layout) {
    QString text="DAISYWHEEL LAYOUT V1\nName = "+layout.name+"\n\n";
    text+=sectionText("letters.lower",layout.lower[0]);
    text+=sectionText("letters.upper",layout.upper[0]);
    text+=sectionText("symbols",layout.lower[1]);
    text+=sectionText("extended.lower",layout.lower[2]);
    text+=sectionText("extended.upper",layout.upper[2]);
    return text.toUtf8();
}
}
