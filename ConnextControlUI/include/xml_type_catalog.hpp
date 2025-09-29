
#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QSet>
#include <QMap>

struct XField {
    QString name, kind;
    bool required = true;
    int  maxLen   = -1;
    int  upperBound = -1;
    QStringList enumVals;
    QString nestedType;
    QString sequenceElementType;  // sequence의 element 타입 ("char", "T_IdentifierType" 등)
    bool isSequenceOfPrimitive = false;  // primitive 타입 배열인지
    bool isSequenceOfString = false;     // 문자열(char sequence)인지
    bool isKey = false;  // key="true" 속성 (primary key)
};

struct XTypeSchema {
    QString name;
    QList<XField> fields;
    bool isNested = false;  // nested="true" 속성
};

class XmlTypeCatalog {
public:
    QSet<QString> parsedFiles;
    QMap<QString, XTypeSchema> typeTable;
    bool parseXmlFile(const QString& path);
    bool hasType(const QString& typeName) const;
    const XTypeSchema& getType(const QString& typeName) const;
    QStringList topicTypeNames() const;
    XTypeSchema resolveType(const QString& typeName) const;  // 타입 해결
    // Validate that all referenced types (nestedType, sequenceElementType) exist in the catalog.
    // Fills messages with warning strings for unresolved references.
    void validateAllReferences(QStringList& messages) const;

private:
    void resolveTypedefChains();  // typedef 체인 해결
};