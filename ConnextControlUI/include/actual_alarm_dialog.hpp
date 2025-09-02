#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QJsonObject>

class ActualAlarmDialog : public QDialog {
    Q_OBJECT
public:
    ActualAlarmDialog(QWidget* parent = nullptr);

    QJsonObject toJson() const;

private:
    QLineEdit* sourceIdRes_;
    QLineEdit* sourceIdInst_;
    QDateTimeEdit* timeGen_;
    QLineEdit* componentName_;
    QLineEdit* nature_;
    QLineEdit* subsystemName_;
    QLineEdit* measure_;
    QDateTimeEdit* dateTimeRaised_;
    QComboBox* alarmState_;
    QLineEdit* raisingCondRes_;
    QLineEdit* raisingCondInst_;
    QLineEdit* alarmCatRes_;
    QLineEdit* alarmCatInst_;
};
