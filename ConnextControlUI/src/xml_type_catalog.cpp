#include <algorithm>
#include <QVector>
#include <QPair>
#include "xml_type_catalog.hpp"
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>

static QString leaf(const QString& nm){
    int p = nm.lastIndexOf("::");
    return p>=0 ? nm.mid(p+2) : nm;
}

QVector<QPair<QString,QString>> listTopicTypesFromXml(const QString& xmlDir){
    QVector<QPair<QString,QString>> out; // {display, full}
    QDir d(xmlDir);
    for (const auto& fn : d.entryList({"*.xml"}, QDir::Files)) {
        QFile f(d.filePath(fn)); if(!f.open(QIODevice::ReadOnly)) continue;
        QXmlStreamReader xr(&f);
        while(!xr.atEnd()){
            xr.readNext();
            if (xr.isStartElement() && xr.name()=="struct") {
                const QString full = xr.attributes().value("name").toString();
                const QString disp = leaf(full);
                if (disp.startsWith("C_")) out.push_back({disp, full});
            }
        }
    }
    std::sort(out.begin(), out.end(), [](auto&a,auto&b){return a.first<b.first;});
    out.erase(std::unique(out.begin(), out.end(), [](auto&a,auto&b){return a.second==b.second;}), out.end());
    return out;
}

static QString normPrim(const QString& t) {
    const QString k = t.toLower();
    if (k=="boolean"||k=="bool") return "bool";
    if (k=="octet"||k=="uint8")  return "uint8";
    if (k=="short"||k=="int16")  return "int16";
    if (k=="unsignedshort"||k=="uint16") return "uint16";
    if (k=="long"||k=="int32")   return "int32";
    if (k=="unsignedlong"||k=="uint32") return "uint32";
    if (k=="longlong"||k=="int64") return "int64";
    if (k=="unsignedlonglong"||k=="uint64") return "uint64";
    if (k=="float")  return "float32";
    if (k=="double") return "float64";
    if (k=="string") return "string";
    return t;
}

static bool parseRtiXml(const QString& path, const QString& wantType, XTypeSchema& out) {
    QFile f(path); if(!f.open(QIODevice::ReadOnly)) return false;
    QXmlStreamReader xr(&f);
    bool inStruct=false, match=false;
    while(!xr.atEnd()){
        xr.readNext();
        if(xr.isStartElement()){
            const auto tag = xr.name().toString();
            if(tag=="struct"){
                const auto name = xr.attributes().value("name").toString();
                inStruct = true;
                match = (wantType.isEmpty() || name==wantType);
                if(match){ out = {}; out.name = name; out.fields.clear(); }
            } else if(inStruct && tag=="member" && match){
                XField fld;
                fld.name = xr.attributes().value("name").toString();
                QString atType = xr.attributes().value("type").toString();
                int depth = 1;
                while(depth>0 && !xr.atEnd()){
                    xr.readNext();
                    if(xr.isStartElement()){
                        const auto tag2 = xr.name().toString();
                        if(tag2=="type"){
                            const auto kind = xr.attributes().value("kind").toString();
                            if(!kind.isEmpty()) atType = kind;
                            const auto mx = xr.attributes().value("maxLength").toInt();
                            if(mx>0) fld.maxLen = mx;
                        } else if(tag2=="string"){
                            fld.kind = "string";
                            const auto mx = xr.attributes().value("maxLength").toInt();
                            if(mx>0) fld.maxLen = mx;
                        } else if(tag2=="enum"){
                            fld.kind = "enum";
                        } else if(tag2=="sequence"){
                            fld.kind = "sequence";
                            const auto ub = xr.attributes().value("maxLength").toInt();
                            if(ub>0) fld.upperBound = ub;
                        } else if(tag2=="elementType"){
                            fld.nestedType = xr.attributes().value("name").toString();
                        } else if(tag2=="required"){
                            const auto v = xr.readElementText();
                            fld.required = (v.trimmed().toLower()=="true");
                        } else if(tag2=="enumerator"){
                            fld.enumVals << xr.attributes().value("name").toString();
                        }
                    } else if(xr.isEndElement()){
                        if(xr.name()=="member") { depth=0; break; }
                    }
                }
                // 가이드 2-1: struct 타입명 채우기
                if (fld.kind.isEmpty()) {
                    auto isPrim = [](const QString& t) {
                        static const QSet<QString> p = {"bool","uint8","int16","uint16","int32","uint32",
                                                        "int64","uint64","float32","float64","string"};
                        return p.contains(t.toLower());
                    };
                    if (atType.startsWith("sequence")) {
                        fld.kind = "sequence";
                        int l = atType.indexOf('<'), r = atType.indexOf('>');
                        if (l>0 && r>l) fld.nestedType = atType.mid(l+1, r-l-1).trimmed();
                    } else if (atType.startsWith("enum")) {
                        fld.kind = "enum";
                    } else if (!atType.isEmpty()) {
                        const QString np = normPrim(atType);
                        if (isPrim(np)) fld.kind = np; // 원시형
                        else { fld.kind = "struct"; fld.nestedType = atType; } // 사용자 정의 타입
                    } else {
                        fld.kind = "struct";
                    }
                }
                out.fields.push_back(fld);
            }
        } else if(xr.isEndElement()){
            if(xr.name()=="struct"){ inStruct=false; if(match) return true; }
        }
    }
    return match;
}

QStringList listTypesFromXmlDir(const QString& xmlDir){
    QDir d(xmlDir);
    QStringList types;
    for (const auto& fn : d.entryList(QStringList() << "*.xml", QDir::Files)) {
        QFile f(d.filePath(fn));
        if (!f.open(QIODevice::ReadOnly)) continue;
        QXmlStreamReader xr(&f);
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isStartElement() && xr.name()=="struct") {
                const auto nm = xr.attributes().value("name").toString();
                if (!nm.isEmpty()) types << nm;   // 예: "P_Alarms_PSM::C_Actual_Alarm"
            }
        }
    }
    types.removeDuplicates();
    types.sort();
    return types;
}

bool loadTypeSchema(const QString& xmlDir, const QString& typeName, XTypeSchema& out){
    const QString p = xmlDir + "/" + typeName + ".xml";
    if(QFile::exists(p) && parseRtiXml(p, typeName, out)) return true;
    QDir d(xmlDir);
    for(const auto& fn: d.entryList(QStringList()<<"*.xml", QDir::Files)){
        if(parseRtiXml(d.filePath(fn), typeName, out)) return true;
    }
    return false;
}
