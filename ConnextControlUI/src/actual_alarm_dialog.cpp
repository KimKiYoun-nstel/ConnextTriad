#include "actual_alarm_dialog.hpp"

ActualAlarmDialog::ActualAlarmDialog(QWidget* parent) : QDialog(parent) {
    auto* layout = new QFormLayout(this);

    sourceIdRes_ = new QLineEdit("1");
    sourceIdInst_ = new QLineEdit("100");
    timeGen_ = new QDateTimeEdit(QDateTime::currentDateTime());
    componentName_ = new QLineEdit("Engine");
    nature_ = new QLineEdit("Overheat");
    subsystemName_ = new QLineEdit("Powertrain");
    measure_ = new QLineEdit("Temperature");
    dateTimeRaised_ = new QDateTimeEdit(QDateTime::currentDateTime());
    alarmState_ = new QComboBox();
    alarmState_->addItems({
        "UNACK",
        "ACK",
        "RESOLVED",
        "DESTROYED",
        "CLEARED"
    });
    raisingCondRes_ = new QLineEdit("2");
    raisingCondInst_ = new QLineEdit("200");
    alarmCatRes_ = new QLineEdit("3");
    alarmCatInst_ = new QLineEdit("300");

    layout->addRow("SourceID ResourceId", sourceIdRes_);
    layout->addRow("SourceID InstanceId", sourceIdInst_);
    layout->addRow("TimeOfDataGeneration", timeGen_);
    layout->addRow("ComponentName", componentName_);
    layout->addRow("Nature", nature_);
    layout->addRow("SubsystemName", subsystemName_);
    layout->addRow("Measure", measure_);
    layout->addRow("DateTimeRaised", dateTimeRaised_);
    layout->addRow("AlarmState", alarmState_);
    layout->addRow("RaisingCondition ResourceId", raisingCondRes_);
    layout->addRow("RaisingCondition InstanceId", raisingCondInst_);
    layout->addRow("AlarmCategory ResourceId", alarmCatRes_);
    layout->addRow("AlarmCategory InstanceId", alarmCatInst_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);
}

QJsonObject ActualAlarmDialog::toJson() const {
    QJsonObject obj;
    obj["sourceID"] = QJsonObject{
        {"resourceId", sourceIdRes_->text().toInt()},
        {"instanceId", sourceIdInst_->text().toInt()}
    };
    obj["timeOfDataGeneration"] = QJsonObject{
        {"second", timeGen_->dateTime().toSecsSinceEpoch()},
        {"nanoseconds", 0}
    };
    obj["componentName"] = componentName_->text();
    obj["nature"] = nature_->text();
    obj["subsystemName"] = subsystemName_->text();
    obj["measure"] = measure_->text();
    obj["dateTimeRaised"] = QJsonObject{
        {"second", dateTimeRaised_->dateTime().toSecsSinceEpoch()},
        {"nanoseconds", 0}
    };
    obj["alarmState"] = alarmState_->currentText();
    obj["raisingConditionSourceID"] = QJsonObject{
        {"resourceId", raisingCondRes_->text().toInt()},
        {"instanceId", raisingCondInst_->text().toInt()}
    };
    obj["alarmCategorySourceID"] = QJsonObject{
        {"resourceId", alarmCatRes_->text().toInt()},
        {"instanceId", alarmCatInst_->text().toInt()}
    };
    return obj;
}
