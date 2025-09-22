#pragma once
#include <QWidget>
#include <QJsonObject>

class FormBuilder {
public:
    static QWidget* buildForm(const QJsonObject& schema, QWidget* parent = nullptr);
};
