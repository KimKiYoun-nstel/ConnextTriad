#pragma once
#include <QWidget>
#include <QJsonObject>

class ValueCollector {
public:
    static QJsonObject collect(QWidget* formRoot);
};
