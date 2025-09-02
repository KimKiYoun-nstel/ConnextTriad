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
        "L_Actual_Alarm_StateType_Unacknowledged",
        "L_Actual_Alarm_StateType_Acknowledged",
        "L_Actual_Alarm_StateType_Resolved",
        "L_Actual_Alarm_StateType_Destroyed",
        "L_Actual_Alarm_StateType_Cleared"
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
    obj["A_sourceID"] = QJsonObject{
        {"A_resourceId", sourceIdRes_->text().toInt()},
        {"A_instanceId", sourceIdInst_->text().toInt()}
    };
    obj["A_timeOfDataGeneration"] = QJsonObject{
        {"A_second", timeGen_->dateTime().toSecsSinceEpoch()},
        {"A_nanoseconds", 0}
    };
    obj["A_componentName"] = componentName_->text();
    obj["A_nature"] = nature_->text();
    obj["A_subsystemName"] = subsystemName_->text();
    obj["A_measure"] = measure_->text();
    obj["A_dateTimeRaised"] = QJsonObject{
        {"A_second", dateTimeRaised_->dateTime().toSecsSinceEpoch()},
        {"A_nanoseconds", 0}
    };
    obj["A_alarmState"] = alarmState_->currentText();
    obj["A_raisingCondition_sourceID"] = QJsonObject{
        {"A_resourceId", raisingCondRes_->text().toInt()},
        {"A_instanceId", raisingCondInst_->text().toInt()}
    };
    obj["A_alarmCategory_sourceID"] = QJsonObject{
        {"A_resourceId", alarmCatRes_->text().toInt()},
        {"A_instanceId", alarmCatInst_->text().toInt()}
    };
    return obj;
}
