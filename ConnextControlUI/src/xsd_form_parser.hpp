#pragma once
#include <QString>
#include <QJsonObject>
#include <vector>
#include <string>

class XmlTypeCatalog {
public:
    static QStringList listTypes(const QString& xmlDir);
    static QJsonObject loadTypeSchema(const QString& xmlDir, const QString& typeName);
};
