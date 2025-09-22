#pragma once
#include <QString>
#include <QStringList>
#include <QList>

struct XField {
    QString name, kind;        // "bool","int32","float64","string","enum","sequence","struct"
    bool required = true;
    int  maxLen   = -1;        // string cap
    int  upperBound = -1;      // sequence cap
    QStringList enumVals;      // enum만 사용
    QString nestedType;        // struct/sequence element
};

struct XTypeSchema {
    QString name;
    QList<XField> fields;
};

QStringList listTypesFromXmlDir(const QString& xmlDir);
bool loadTypeSchema(const QString& xmlDir, const QString& typeName, XTypeSchema& out);
QVector<QPair<QString,QString>> listTopicTypesFromXml(const QString& xmlDir);