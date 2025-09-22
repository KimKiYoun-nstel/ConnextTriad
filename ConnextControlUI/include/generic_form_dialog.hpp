#pragma once
#include <QDialog>
#include <QJsonObject>
#include "xml_type_catalog.hpp"

class GenericFormDialog : public QDialog {
    Q_OBJECT
public:
    GenericFormDialog(const QString& xmlDir, const QString& typeName, QWidget* parent=nullptr);
    QJsonObject toJson() const;

private:
    QWidget* build(const XTypeSchema& s);
    void collect(QWidget* w, const XTypeSchema& s, QJsonObject& out) const;
    QWidget* makeWidget(const XField& f) const;

    QString xmlDir_, type_;
    XTypeSchema schema_;
    QWidget* formRoot_ = nullptr;
};
