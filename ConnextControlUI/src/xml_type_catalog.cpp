#include <algorithm>
#include <QVector>
#include <QPair>
#include "xml_type_catalog.hpp"
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>
#include <QSet>
#include <QMap>
#include <QFileInfo>

// Helper function to check if a type is primitive
bool isPrimitiveType(const QString& typeName) {
    static const QStringList primitives = {
        "bool", "boolean", "char", "wchar", "octet",
        "short", "unsigned short", "long", "unsigned long", 
        "long long", "unsigned long long",
        "float", "double", "long double",
        "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64"
    };
    return primitives.contains(typeName) || primitives.contains(typeName.split("::").last());
}

// XmlTypeCatalog 클래스 구현
bool XmlTypeCatalog::parseXmlFile(const QString& path) {
    if (parsedFiles.contains(path)) return true; // 이미 파싱된 파일은 성공으로 처리
    parsedFiles.insert(path);
    QFile f(path); 
    if(!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open XML file:" << path;
        return false;
    }
    
    QXmlStreamReader xr(&f);
    QString currentModule = "";  // 현재 모듈 추적
    
    while(!xr.atEnd()) {
        xr.readNext();
        if(xr.isStartElement()) {
            const auto tag = xr.name().toString();
            
            if(tag=="include") {
                QString incFile = xr.attributes().value("file").toString();
                if (!incFile.isEmpty()) {
                    // 상대 경로 처리: 현재 파일의 디렉토리 기준
                    if (!QFile::exists(incFile) && !path.isEmpty()) {
                        QDir currentDir = QFileInfo(path).dir();
                        QString fullPath = currentDir.absoluteFilePath(incFile);
                        if (QFile::exists(fullPath)) {
                            incFile = fullPath;
                        }
                    }
                    parseXmlFile(incFile); // 중복 방지
                }
            } else if(tag=="module") {
                currentModule = xr.attributes().value("name").toString();
            } else if(tag=="struct") {
                XTypeSchema schema;
                QString structName = xr.attributes().value("name").toString();
                schema.name = currentModule.isEmpty() ? structName : (currentModule + "::" + structName);
                schema.isNested = (xr.attributes().value("nested").toString() == "true");

                while(!xr.atEnd()) {
                    xr.readNext();
                    if(xr.isStartElement() && xr.name()=="member") {
                        XField fld;
                        fld.name = xr.attributes().value("name").toString();
                        QString atType = xr.attributes().value("type").toString();
                        QString nonBasicTypeName = xr.attributes().value("nonBasicTypeName").toString();
                        QString stringMaxLen = xr.attributes().value("stringMaxLength").toString();
                        QString sequenceMaxLen = xr.attributes().value("sequenceMaxLength").toString();
                        fld.isKey = (xr.attributes().value("key").toString() == "true");

                        // 기준에 따라 분기
                        if (atType == "string") {
                            fld.kind = "string";
                            fld.nestedType = "";
                            if (!stringMaxLen.isEmpty()) fld.maxLen = stringMaxLen.toInt();
                        } else if (atType == "nonBasic") {
                            // 시퀀스 타입
                            if (!sequenceMaxLen.isEmpty()) {
                                fld.kind = "sequence";
                                fld.sequenceElementType = nonBasicTypeName;
                                fld.upperBound = sequenceMaxLen.toInt();
                                if (nonBasicTypeName.contains("T_Char") || nonBasicTypeName == "char") {
                                    fld.isSequenceOfString = true;
                                    fld.nestedType = "";
                                } else if (isPrimitiveType(nonBasicTypeName)) {
                                    fld.isSequenceOfPrimitive = true;
                                    fld.nestedType = "";
                                } else {
                                    // 내부 구조체 시퀀스
                                    fld.nestedType = nonBasicTypeName;
                                    fld.isSequenceOfPrimitive = false;
                                    fld.isSequenceOfString = false;
                                }
                            } else {
                                // enum/struct/복합타입
                                // enum 타입 판별은 별도 처리 필요 (여기서는 struct로 기본 처리)
                                fld.kind = "struct";
                                fld.nestedType = nonBasicTypeName;
                            }
                        } else {
                            // 프리미티브 타입
                            fld.kind = atType;
                            fld.nestedType = "";
                        }

                        // 기타 속성 보완
                        if (!stringMaxLen.isEmpty() && atType == "string") {
                            fld.maxLen = stringMaxLen.toInt();
                        }
                        if (!sequenceMaxLen.isEmpty() && atType == "nonBasic") {
                            fld.upperBound = sequenceMaxLen.toInt();
                        }

                        schema.fields.push_back(fld);
                    } else if(xr.isEndElement() && xr.name()=="struct") {
                        break;
                    }
                }
                typeTable[schema.name] = schema;
            } else if(tag=="typedef") {
                // typedef 처리
                QString typedefName = xr.attributes().value("name").toString();
                typedefName = currentModule.isEmpty() ? typedefName : (currentModule + "::" + typedefName);
                const auto basicType = xr.attributes().value("type").toString();
                const auto seqMaxLen = xr.attributes().value("sequenceMaxLength").toString();
                const auto nonBasic = xr.attributes().value("nonBasicTypeName").toString();
                
                XTypeSchema schema;
                schema.name = typedefName;
                XField fld;
                fld.name = "value";  // typedef는 단일 값
                
                if (!seqMaxLen.isEmpty() && !nonBasic.isEmpty()) {
                    // sequence typedef (T_ShortString 등)
                    fld.kind = "sequence";
                    fld.upperBound = seqMaxLen.toInt();
                    fld.sequenceElementType = nonBasic;
                    
                    if (nonBasic.contains("T_Char") || nonBasic == "char") {
                        fld.isSequenceOfString = true;
                    } else if (isPrimitiveType(nonBasic)) {
                        fld.isSequenceOfPrimitive = true;
                    } else {
                        fld.nestedType = nonBasic;
                    }
                } else if (!basicType.isEmpty()) {
                    // 기본 타입 typedef (T_Double, T_Int32 등)
                    if(basicType == "float64" || basicType == "double") fld.kind = "double";
                    else if(basicType == "float32" || basicType == "float") fld.kind = "float";
                    else if(basicType.contains("int")) fld.kind = basicType;
                    else if(basicType == "boolean" || basicType == "bool") fld.kind = "bool";
                    else if(basicType == "char8" || basicType == "char") fld.kind = "string";
                    else fld.kind = basicType;
                } else if (!nonBasic.isEmpty()) {
                    // 복합 타입 typedef (T_HeartbeatType 등)
                    fld.kind = "struct";
                    fld.nestedType = nonBasic;
                }
                
                schema.fields.push_back(fld);
                typeTable[schema.name] = schema;
            } else if(tag=="enum") {
                // 독립 enum 처리
                XTypeSchema schema;
                QString enumName = xr.attributes().value("name").toString();
                schema.name = currentModule.isEmpty() ? enumName : (currentModule + "::" + enumName);
                XField enumField;
                enumField.name = "value";
                enumField.kind = "enum";
                
                while(!xr.atEnd()) {
                    xr.readNext();
                    if(xr.isStartElement() && xr.name()=="enumerator") {
                        enumField.enumVals << xr.attributes().value("name").toString();
                    } else if(xr.isEndElement() && xr.name()=="enum") {
                        break;
                    }
                }
                schema.fields.push_back(enumField);
                typeTable[schema.name] = schema;
            }
        }
    }
    
    // 파싱 완료 후 typedef 체인 해결
    resolveTypedefChains();
    return true;
}

bool XmlTypeCatalog::hasType(const QString& typeName) const {
    return typeTable.contains(typeName);
}

const XTypeSchema& XmlTypeCatalog::getType(const QString& typeName) const {
    static const XTypeSchema emptySchema;  // 기본값으로 사용할 빈 스키마
    auto it = typeTable.find(typeName);
    if (it != typeTable.end()) {
        return it.value();
    }
    return emptySchema;
}

QStringList XmlTypeCatalog::topicTypeNames() const {
    QStringList out;
    for (const auto& k : typeTable.keys()) {
        QString leaf = k.split("::").last();  // 네임스페이스 제거
        if (leaf.startsWith("C_")) out << k;  // 전체 이름 반환 (표시용은 leaf)
    }
    return out;
}

void XmlTypeCatalog::resolveTypedefChains() {
    // typedef 체인을 해결하여 최종 타입으로 변환
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto it = typeTable.begin(); it != typeTable.end(); ++it) {
            const QString& typeName = it.key();
            XTypeSchema& schema = it.value();
            for (auto& field : schema.fields) {
                if (field.kind == "struct" && !field.nestedType.isEmpty()) {
                    // nestedType이 typedef일 경우 해결
                    if (hasType(field.nestedType)) {
                        const auto& nestedSchema = getType(field.nestedType);
                        if (nestedSchema.fields.size() == 1) {
                            // 안전한 복사본 사용 - QList 참조 위험 방지
                            const auto nestedField = nestedSchema.fields.first();
                            if (nestedField.kind != "struct") {
                                // typedef의 실제 타입으로 교체
                                field.kind = nestedField.kind;
                                if (nestedField.kind == "sequence") {
                                    field.upperBound = nestedField.upperBound;
                                    field.sequenceElementType = nestedField.sequenceElementType;
                                    field.isSequenceOfString = nestedField.isSequenceOfString;
                                    field.isSequenceOfPrimitive = nestedField.isSequenceOfPrimitive;
                                }
                                if (!nestedField.nestedType.isEmpty()) {
                                    field.nestedType = nestedField.nestedType;
                                } else {
                                    field.nestedType.clear();
                                }
                                field.enumVals = nestedField.enumVals;
                                changed = true;
                            }
                        }
                    }
                }
            }
        }
    }
}

XTypeSchema XmlTypeCatalog::resolveType(const QString& typeName) const {
    if (!hasType(typeName)) return XTypeSchema{};
    const auto& schema = getType(typeName);
    // typedef/체인을 따라가면서 최종 실제 타입 찾기
    if (schema.fields.size() == 1) {
        const auto field = schema.fields.first();
        // sequence typedef (ex: T_ShortString 등)
        if (field.kind == "sequence" && !field.sequenceElementType.isEmpty()) {
            // 시퀀스 원소 타입의 최종 타입 반환
            return resolveType(field.sequenceElementType);
        }
        // struct typedef (ex: T_HeartbeatType 등)
        if (field.kind == "struct" && !field.nestedType.isEmpty() && hasType(field.nestedType)) {
            return resolveType(field.nestedType);
        }
        // enum/primitive/string 등은 그대로 반환
        // (enum: kind=="enum", primitive: kind=="int32" 등, string: kind=="string")
    }
    return schema;
}