#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QtWidgets>
#include <QTimer>
#include "xml_type_catalog.hpp"

class GenericFormDialog : public QDialog {
    Q_OBJECT
public:
    GenericFormDialog(XmlTypeCatalog* catalog, const QString& typeName, QWidget* parent = nullptr);
    QJsonObject toJson() const;
    static bool getFormResult(XmlTypeCatalog* catalog, const QString& typeName, QWidget* parent, QJsonObject& outResult);

private slots:
    void buildFormDelayed();

private:
    XmlTypeCatalog* catalog_;
    QString typeName_;
    QWidget* formRoot_;
         QJsonObject cachedResult_; // OK 버튼 시점에 결과 캐싱
    
    // 내부 메소드들
    void showErrorForm(const QString& message);
    void setupNormalForm();
    QWidget* buildForm(const XTypeSchema& schema);
    QWidget* makeFieldWidget(const XField& field, QWidget* parent = nullptr);
    QVariant getDefaultValue(const XField& field) const;
    QJsonObject collect(QWidget* widget, const XTypeSchema& schema) const;
    
    // 헬퍼 메소드들 (JSON 변환 개선)
    QWidget* findFieldWidget(QWidget* parent, const QString& objectName) const;
    QJsonValue parseSequenceValue(const QString& text, const XField& field) const;
};
