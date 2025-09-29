#include "../include/mainwindow.hpp"


#include <QtWidgets>

#include <nlohmann/json.hpp>  // 수신 CBOR → JSON pretty 로그에 사용
#include "../include/xml_type_catalog.hpp"
#include "../include/generic_form_dialog.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


using dkmrtp::ipc::Endpoint;
using dkmrtp::ipc::Header;
using dkmrtp::ipc::Role;

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    // 먼저 UI 설정
    setupUi();
    
    // XmlTypeCatalog 초기화 및 XML 로딩
    QString xmlDir = QCoreApplication::applicationDirPath() + "/" + IDL_XML_DIR_RELATIVE;
    QDir dir(xmlDir);
    
    appendLog(QString("Looking for XML files in: %1").arg(xmlDir));
    appendLog(QString("Directory exists: %1").arg(dir.exists() ? "Yes" : "No"));
    
    if (dir.exists()) {
        QStringList xmlFiles = dir.entryList({"*.xml"}, QDir::Files);
        appendLog(QString("Found %1 XML files").arg(xmlFiles.size()));
        
        for (const QString& file : xmlFiles) {
            QString filePath = dir.absoluteFilePath(file);
            appendLog(QString("Parsing XML: %1").arg(filePath));
            bool success = catalog_.parseXmlFile(filePath);
            appendLog(QString("Parse result: %1").arg(success ? "Success" : "Failed"));
        }
    }

    // Validate cross-type references (report unresolved nested/sequence references)
    QStringList refWarnings;
    catalog_.validateAllReferences(refWarnings);
    for (const QString& w : refWarnings) appendLog(QString("[WRN] %1").arg(w));

    // Verbose diagnostic dump: list types, their fields, resolved schemas and generated sample JSON
    const bool doVerboseDump = true; // set to false to disable
    if (doVerboseDump) {
        appendLog(QString("--- TypeTable Diagnostic Dump (count=%1) ---").arg(catalog_.typeTable.size()));
        for (const QString& tname : catalog_.typeTable.keys()) {
            appendLog(QString("[DBG] Type: %1").arg(tname));
            const XTypeSchema& s = catalog_.getType(tname);
            for (const XField& f : s.fields) {
                appendLog(QString("[DBG]   field: %1 kind=%2 nested=%3 seqElem=%4 isSeqStr=%5 isSeqPrim=%6 enumCount=%7")
                          .arg(f.name)
                          .arg(f.kind)
                          .arg(f.nestedType)
                          .arg(f.sequenceElementType)
                          .arg(f.isSequenceOfString)
                          .arg(f.isSequenceOfPrimitive)
                          .arg(f.enumVals.size()));
            }
            // resolved schema
            XTypeSchema resolved = catalog_.resolveType(tname);
            if (resolved.fields.isEmpty()) {
                appendLog(QString("[DBG]   resolved: <empty> for %1").arg(tname));
            } else {
                appendLog(QString("[DBG]   resolved schema for %1: fields=%2").arg(tname).arg(resolved.fields.size()));
                for (const XField& rf : resolved.fields) {
                    appendLog(QString("[DBG]     -> %1 kind=%2 nested=%3 seqElem=%4 isSeqStr=%5 isSeqPrim=%6 enumCount=%7")
                              .arg(rf.name).arg(rf.kind).arg(rf.nestedType).arg(rf.sequenceElementType).arg(rf.isSequenceOfString).arg(rf.isSequenceOfPrimitive).arg(rf.enumVals.size()));
                }
            }
            // sample json
            QJsonObject sample = generateSampleJsonObject(tname);
            QJsonDocument doc(sample);
            appendLog(QString("[DBG]   sample: %1").arg(QString::fromUtf8(doc.toJson(QJsonDocument::Compact))));
        }
        appendLog(QString("--- End Diagnostic Dump ---"));
    }
    
    // 파싱 완료 후 타입 개수 확인
    QStringList allTypes = catalog_.topicTypeNames();
    appendLog(QString("Total types loaded: %1").arg(allTypes.size()));
    
    // 업데이트된 타입 목록 확인
    for (const QString& typeName : allTypes) {
        appendLog(QString("- Type: %1").arg(typeName));
    }
    
    // XML 파싱 완료 후 콤보박스 업데이트
    updateTypeComboBoxes();

    // IPC 콜백 설치(레거시 + 새 프레임)
    dkmrtp::ipc::DkmRtpIpc::Callbacks cb{};

    // 새 경로(REQ/RSP/EVT = CBOR 페이로드)
    cb.on_response = [this](const Header& h, const uint8_t* body, uint32_t len) {
        try {
            auto j = nlohmann::json::from_cbor(std::vector<uint8_t>(body, body + len));
            // EVT 수신 표시부: display 역변환
            if (j.is_object() && j.value("evt","")=="data" && j.contains("display")) {
                const std::string type = j.value("type","");
                // XML 메타 기반 역변환 (필요시 구현)
            }
            QString logLine;
            if (j.value("ok", false)) {
                // 성공 응답: result/action 등 표시
                if (j.contains("result")) {
                    auto result = j["result"];
                    QString action = QString::fromStdString(result.value("action", ""));
                    logLine = QString("[RSP] id=%1 action=%2").arg(h.corr_id).arg(action);
                    // 파라미터 정보 추가
                    if (result.contains("domain")) logLine += QString(" domain=%1").arg(result["domain"].get<int>());
                    if (result.contains("publisher"))
                        logLine += QString(" publisher=%1")
                                       .arg(QString::fromStdString(result["publisher"].get<std::string>()));
                } else {
                    logLine = QString("[RSP] id=%1 ok=true").arg(h.corr_id);
                }
            } else {
                // 실패 응답: category/msg 등 표시
                int category = j.value("category", -1);
                QString msg = QString::fromStdString(j.value("msg", ""));
                QString catStr = (category == 1) ? "[Resource]" : (category == 2) ? "[Logic]" : "[Unknown]";
                logLine = QString("[RSP] id=%1 error=%2 %3").arg(h.corr_id).arg(catStr).arg(msg);
            }
            QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection, Q_ARG(QString, logLine),
                                      Q_ARG(bool, false));
            QMetaObject::invokeMethod(this->statusBar(), "showMessage", Qt::QueuedConnection, Q_ARG(QString, logLine),
                                      Q_ARG(int, 2000));
        } catch (...) {
            QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
                                      Q_ARG(QString, QString("[RSP] id=%1 <CBOR parse error>").arg(h.corr_id)),
                                      Q_ARG(bool, false));
        }
    };
    cb.on_event = [this](const Header&, const uint8_t* body, uint32_t len) {
        try {
            auto j = nlohmann::json::from_cbor(std::vector<uint8_t>(body, body + len));
            QString topic = QString::fromStdString(j.value("topic", ""));
            QString type = QString::fromStdString(j.value("type", ""));
            bool display_present = j.contains("display") && !j["display"].is_null();
            QString pretty_json_truncated = QString::fromStdString(j.dump(2));
            if (pretty_json_truncated.size() > 2048) pretty_json_truncated = pretty_json_truncated.left(2048) + "...";
            appendLog(QString("[EVT] topic=%1 type=%2 has_display=%3 json=%4")
                .arg(topic).arg(type).arg(display_present).arg(pretty_json_truncated));
        } catch (...) {
            QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
                                      Q_ARG(QString, QString("[EVT] <CBOR parse error>")), Q_ARG(bool, false));
        }
    };

    ipc_.set_callbacks(cb);
    updateUiState();
}

void MainWindow::updateTypeComboBoxes() {
    QStringList typeNames = catalog_.topicTypeNames();
    
    // topicCombo_ 업데이트
    if (topicCombo_) {
        topicCombo_->clear();
        for (const QString& typeName : typeNames) {
            topicCombo_->addItem(typeName, typeName);
        }
        appendLog(QString("topicCombo_ populated with %1 types").arg(topicCombo_->count()));
    }
    
    // cbType_ 업데이트  
    if (cbType_) {
        cbType_->clear();
        for (const QString& typeName : typeNames) {
            cbType_->addItem(typeName, typeName);
        }
        appendLog(QString("cbType_ populated with %1 types").arg(cbType_->count()));
    }
    
    // Open Form 버튼 연결 (XML 파싱 완료 후)
    static bool connected = false;
    if (!connected && btnOpenForm_) {
        connect(btnOpenForm_, &QPushButton::clicked, this, [this]() {
            QString selectedType = topicCombo_->currentText();
            appendLog(QString("Open Form clicked, selected type: %1").arg(selectedType));
            if (!selectedType.isEmpty()) {
                onTopicSelected(selectedType);
            } else {
                appendLog("No type selected");
            }
        });
        connected = true;
        appendLog("Open Form button connected successfully");
    }
}

// Helper: create a sample JSON value for a given XField according to xml_parsing_rule.md and 개발가이드
// Helper: create a sample JSON value for a given XField according to xml_parsing_rule.md and 개발가이드
// This variant accepts a MainWindow* so it can route warnings to the UI log (appendLog).
static QJsonValue makeSampleForField(const XmlTypeCatalog& catalog, const XField& fld, int& idCounter, MainWindow* owner) {
    // Remove A_ prefix for string tokens when creating friendly defaults
    auto tokenFromName = [](const QString& name) {
        if (name.startsWith("A_")) return name.mid(2);
        return name;
    };

    // Prefer explicit kind-based handling first. If metadata is inconsistent (e.g. both
    // kind=="string" and isSequenceOfString==true) prefer 'kind' but also warn.
    // Normalize kind string for flexible matching (handles T_* typedef names)
    const QString lk = fld.kind.toLower();

    if (lk == "string" || lk.contains("char")) {
        if (fld.isSequenceOfString && owner) owner->appendLog(QString("[WRN] Field %1: has kind=string but also marked isSequenceOfString; treating as string").arg(fld.name));
        return QJsonValue(tokenFromName(fld.name));
    }

    if (lk == "enum") {
        if (!fld.enumVals.isEmpty()) return QJsonValue(fld.enumVals.first());
        return QJsonValue(QString());
    }

    if (lk.contains("int") || lk.contains("uint")) {
        return QJsonValue(1);
    }

    if (lk.contains("float") || lk.contains("double")) {
        return QJsonValue(1.0);
    }

    if (lk == "boolean" || lk == "bool" || lk.contains("t_boolean") || lk.contains("t_boolean") || lk.contains("t_boolean")) {
        return QJsonValue(false);
    }

    // Sequence checks handled after primitive/string/enum handling
    if (fld.isSequenceOfString) {
        // string sequence -> produce one string element
        return QJsonArray{QJsonValue(tokenFromName(fld.name))};
    }

    if (fld.isSequenceOfPrimitive) {
        // primitive sequence -> produce one element depending on sequenceElementType
        QJsonArray arr;
        const QString elem = fld.sequenceElementType;
        if (elem.startsWith("int") || elem.startsWith("uint")) arr.append(1);
        else if (elem.startsWith("float") || elem == "double") arr.append(1.0);
        else arr.append(1);
        return arr;
    }

    if (fld.kind.startsWith("int") || fld.kind == "int32" || fld.kind == "int16" || fld.kind == "int64") {
        return QJsonValue(1);
    }

    if (fld.kind == "float" || fld.kind == "double" || fld.kind == "float32" || fld.kind == "float64") {
        return QJsonValue(1.0);
    }

    if (fld.kind == "boolean" || fld.kind == "T_Boolean") {
        return QJsonValue(false);
    }

    if (fld.kind == "enum") {
        if (!fld.enumVals.isEmpty()) return QJsonValue(fld.enumVals.first());
        return QJsonValue(QString());
    }

    if (fld.kind == "struct") {
        // Resolve nested type schema and recurse. If the resolved type is a single-field typedef
        // to a primitive/string/sequence, unwrap and emit the primitive directly (to avoid
        // emitting an object where a string/primitive is expected).
        XTypeSchema resolved = catalog.resolveType(fld.nestedType);
        if (resolved.fields.size() == 1) {
            const XField& sf = resolved.fields.first();
            // If the single nested field is effectively primitive/string/sequence, unwrap it
            const bool isPrimitiveLike = (sf.kind == "string" || sf.isSequenceOfString || sf.isSequenceOfPrimitive || sf.kind.startsWith("int") || sf.kind.startsWith("uint") || sf.kind.startsWith("float") || sf.kind == "double" || sf.kind == "boolean" || sf.kind == "T_Boolean" || sf.kind == "enum");
            if (isPrimitiveLike) {
                if (owner) owner->appendLog(QString("[WRN] Field %1: resolved struct %2 is a single-field typedef; emitting primitive for compatibility").arg(fld.name).arg(fld.nestedType));
                return makeSampleForField(catalog, sf, idCounter, owner);
            }
        }

        QJsonObject obj;
        for (const XField& nf : resolved.fields) {
            obj.insert(nf.name, makeSampleForField(catalog, nf, idCounter, owner));
        }
        return obj;
    }

    if (fld.kind == "sequence") {
        QJsonArray arr;
        if (!fld.nestedType.isEmpty()) {
            // sequence of struct - but the nested struct may be a single-field typedef -> unwrap
            XTypeSchema resolved = catalog.resolveType(fld.nestedType);
            if (resolved.fields.size() == 1) {
                const XField& sf = resolved.fields.first();
                const bool isPrimitiveLike = (sf.kind == "string" || sf.isSequenceOfString || sf.isSequenceOfPrimitive || sf.kind.startsWith("int") || sf.kind.startsWith("uint") || sf.kind.startsWith("float") || sf.kind == "double" || sf.kind == "boolean" || sf.kind == "T_Boolean" || sf.kind == "enum");
                if (isPrimitiveLike) {
                    if (owner) owner->appendLog(QString("[WRN] Field %1: sequence of %2 resolves to single-field typedef; emitting sequence of primitive").arg(fld.name).arg(fld.nestedType));
                    arr.append(makeSampleForField(catalog, sf, idCounter, owner));
                    return arr;
                }
            }

            QJsonObject elem;
            for (const XField& nf : resolved.fields) elem.insert(nf.name, makeSampleForField(catalog, nf, idCounter, owner));
            arr.append(elem);
            return arr;
        } else if (!fld.sequenceElementType.isEmpty()) {
            // already handled isSequenceOfPrimitive/isSequenceOfString above; fallback
            arr.append(1);
            return arr;
        }
    }

    // If field kind is struct or otherwise unresolved, try resolving nestedType if present
    if (!fld.nestedType.isEmpty()) {
        XTypeSchema resolved = catalog.resolveType(fld.nestedType);
        if (!resolved.fields.isEmpty()) {
            // If resolved is a single-field typedef, unwrap
            if (resolved.fields.size() == 1) {
                return makeSampleForField(catalog, resolved.fields.first(), idCounter, owner);
            }
            QJsonObject obj;
            for (const XField& nf : resolved.fields) obj.insert(nf.name, makeSampleForField(catalog, nf, idCounter, owner));
            return obj;
        } else {
            if (owner) owner->appendLog(QString("[WRN] makeSampleForField: unable to resolve nestedType '%1' for field %2").arg(fld.nestedType).arg(fld.name));
        }
    }

    // Fallback: log unresolved kind and return empty string (user can edit payload)
    if (owner) owner->appendLog(QString("[WRN] makeSampleForField: unresolved field %1 kind='%2' nestedType='%3'").arg(fld.name).arg(fld.kind).arg(fld.nestedType));
    return QJsonValue(QString());
}

// Implementation of member function declared in header: generateSampleJsonObject
QJsonObject MainWindow::generateSampleJsonObject(const QString& typeName) {
    QJsonObject out;
    // basic guard
    if (!catalog_.hasType(typeName)) {
        appendLog(QString("[WRN] generateSampleJsonObject: unknown type %1").arg(typeName));
        return out;
    }

    XTypeSchema schema = catalog_.resolveType(typeName);
    if (schema.fields.empty()) {
        appendLog(QString("[WRN] generateSampleJsonObject: resolved schema empty for %1").arg(typeName));
        return out;
    }
    int idCounter = sampleIdCounter_;
    for (const XField& f : schema.fields) {
        // Use a copy of idCounter so nested calls can increment consistently
        out.insert(f.name, makeSampleForField(catalog_, f, idCounter, this));
    }
    // update persistent counter so next sample increments ids
    sampleIdCounter_ = idCounter + 1;
    return out;
}

void MainWindow::onTopicSelected(const QString& typeName) {
    if (typeName.isEmpty()) return;
    
    if (!catalog_.hasType(typeName)) {
        appendLog(QString("Type not found: %1").arg(typeName));
        return;
    }
    
    // 힙에서 생성하여 스택 오버플로우 방지
        QJsonObject json;
        if (GenericFormDialog::getFormResult(&catalog_, typeName, this, json)) {
            QJsonDocument doc(json);
            tePayload_->setPlainText(doc.toJson(QJsonDocument::Compact));
            appendLog("Form completed successfully");
        }
}

void MainWindow::setupUi()
{
    setWindowTitle("Connext Control UI");
    resize(900, 560);

    auto* w = new QWidget;
    setCentralWidget(w);
    auto* lay = new QVBoxLayout(w);

    // 버튼 시인성 개선: 공통 스타일 적용
    const QString btnStyle =
        "QPushButton {"
        "  background-color: #e3eafc;"
        "  border: 1.5px solid #90caf9;"
        "  border-radius: 6px;"
        "  padding: 6px 16px;"
        "  font-weight: bold;"
        "  color: #1a237e;"
        "}"
        "QPushButton:hover {"
        "  background-color: #bbdefb;"
        "  border: 2px solid #1976d2;"
        "  color: #0d1335;"
        "}";

    // 연결 영역
    auto* gbConn = new QGroupBox("Connection");
    auto* cl = new QHBoxLayout(gbConn);
    leRole_ = new QLineEdit("client");
    leAddr_ = new QLineEdit("127.0.0.1");
    lePort_ = new QLineEdit("25000");
    btnConn_ = new QPushButton("Connect");
    cl->addWidget(new QLabel("role"));
    cl->addWidget(leRole_);
    cl->addWidget(new QLabel("addr"));
    cl->addWidget(leAddr_);
    cl->addWidget(new QLabel("port"));
    cl->addWidget(lePort_);
    cl->addWidget(btnConn_);
    lay->addWidget(gbConn);

    // 토픽 타입 선택 영역 추가
    auto* gbTopic = new QGroupBox("Topic Type Selection");
    auto* tl = new QHBoxLayout(gbTopic);
    topicCombo_ = new QComboBox();
    // 파싱 후에 updateTypeComboBoxes()에서 채움
    btnOpenForm_ = new QPushButton("Open Form");
    tl->addWidget(new QLabel("Type:"));
    tl->addWidget(topicCombo_);
    tl->addWidget(btnOpenForm_);
    tl->addStretch();
    
    // NOTE: The UI requirement is to hide the Topic Type Selection area while preserving
    // underlying functionality. We therefore keep the group in the widget tree but hide it.
    gbTopic->setVisible(false);
    btnOpenForm_->setVisible(false);

    // connect is handled in updateTypeComboBoxes()
    lay->addWidget(gbTopic);

    // Participant
    auto* gbPart = new QGroupBox("Participant");
    auto* pl = new QHBoxLayout(gbPart);
    leDomain_ = new QLineEdit("0");
    leQosLib_ = new QLineEdit("TriadQosLib");
    leQosProf_ = new QLineEdit("DefaultReliable");
    btnPart_ = new QPushButton("Create");
    pl->addWidget(new QLabel("domain"));
    pl->addWidget(leDomain_);
    pl->addWidget(new QLabel("qosLib"));
    pl->addWidget(leQosLib_);
    pl->addWidget(new QLabel("qosProf"));
    pl->addWidget(leQosProf_);
    pl->addWidget(btnPart_);
    lay->addWidget(gbPart);

    // Pub/Sub
    auto* gbIO = new QGroupBox("Pub/Sub");
    auto* il = new QGridLayout(gbIO);
    leTopic_ = new QLineEdit("HelloTopic");
    cbType_ = new QComboBox();
    cbType_->setEditable(false); // user should not edit type text
    // 파싱 후에 updateTypeComboBoxes()에서 채움

    lePubName_ = new QLineEdit("pub1");
    leSubName_ = new QLineEdit("sub1");
    // Payload must be editable by the user; make it writable
    tePayload_ = new QTextEdit("{}");
    tePayload_->setReadOnly(false);
    tePayload_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // Add a small button beside payload to open a popup dialog with a large editor
    btnPayloadPopup_ = new QPushButton("Open Payload");
    btnWriter_ = new QPushButton("Create Writer");
    btnReader_ = new QPushButton("Create Reader");
    btnPub_ = new QPushButton("Publish");
    btnPublisher_ = new QPushButton("Create Publisher");
    btnSubscriber_ = new QPushButton("Create Subscriber");
    int r = 0;
    il->addWidget(new QLabel("topic"), r, 0);
    il->addWidget(leTopic_, r, 1);
    r++;
    il->addWidget(new QLabel("type"), r, 0);
    il->addWidget(cbType_, r, 1);
    r++;
    il->addWidget(new QLabel("publisher"), r, 0);
    il->addWidget(lePubName_, r, 1);
    il->addWidget(btnPublisher_, r, 2);
    r++;
    il->addWidget(new QLabel("subscriber"), r, 0);
    il->addWidget(leSubName_, r, 1);
    il->addWidget(btnSubscriber_, r, 2);
    r++;
    il->addWidget(btnWriter_, r, 0);
    il->addWidget(btnReader_, r, 1);
    r++;
    il->addWidget(new QLabel("payload"), r, 0);
    il->addWidget(tePayload_, r, 1);
    il->addWidget(btnPayloadPopup_, r, 2);
    r++;
    il->addWidget(btnPub_, r, 1);
    // Make the IO and Log sections resizable via a vertical splitter so payload can be resized by user
    auto* splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(gbIO);
    // Log level + 로그 패널
    auto* logContainer = new QWidget;
    auto* logLayout = new QVBoxLayout(logContainer);

    // Log level + 로그 패널
    auto* ll = new QHBoxLayout();
    cbLogLevel_ = new QComboBox();
    cbLogLevel_->addItems({"Info", "Debug"});
    ll->addWidget(new QLabel("Log Level"));
    ll->addWidget(cbLogLevel_);
    ll->addStretch();
    lay->addLayout(ll);

    teLog_ = new QTextEdit();
    teLog_->setReadOnly(true);
    teLog_->setMinimumHeight(160);
    btnClearLog_ = new QPushButton("Clear Log");
    logLayout->addWidget(teLog_);
    logLayout->addWidget(btnClearLog_);
    logContainer->setLayout(logLayout);

    splitter->addWidget(logContainer);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    lay->addWidget(splitter);

    // DDS 엔티티 전체 초기화 버튼
    btnClearDds_ = new QPushButton("Clear DDS Entities");
    lay->addWidget(btnClearDds_);


    // 연결
    connect(btnConn_, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(btnPart_, &QPushButton::clicked, this, &MainWindow::onCreateParticipant);
    connect(btnPublisher_, &QPushButton::clicked, this, &MainWindow::onCreatePublisher);
    connect(btnSubscriber_, &QPushButton::clicked, this, &MainWindow::onCreateSubscriber);
    connect(btnWriter_, &QPushButton::clicked, this, &MainWindow::onCreateWriter);
    connect(btnReader_, &QPushButton::clicked, this, &MainWindow::onCreateReader);
    connect(btnPub_, &QPushButton::clicked, this, &MainWindow::onPublishSample);
    connect(cbLogLevel_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onLogLevelChanged);
    // When a type is selected or the text changed in the Pub/Sub 'type' combo,
    // auto-generate a sample JSON for types that start with 'C_' and populate
    // the payload editor so the user can modify it.
    auto populateFromCombo = [this]() {
        // Prefer the explicit userData (currentData) if present, otherwise use the visible text.
        QString fullTypeName = cbType_->currentData().toString();
        if (fullTypeName.isEmpty()) fullTypeName = cbType_->currentText();
        fullTypeName = fullTypeName.trimmed();
        // The stored value is a fully-qualified name (e.g. P_Alarms_PSM::C_Actual_Alarm).
        // Check only the leaf (after ::) for the C_ prefix, but use fullTypeName for lookup.
        const QString leaf = fullTypeName.split("::").last();
        if (leaf.startsWith("C_")) {
            QJsonObject sample = generateSampleJsonObject(fullTypeName);
            QJsonDocument doc(sample);
            tePayload_->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
            appendLog(QString("Sample JSON populated for type %1").arg(fullTypeName));
            // Update topic name to type's leaf without C_ prefix
            const QString topicName = leaf.mid(2); // remove leading C_
            if (!topicName.isEmpty()) {
                leTopic_->setText(topicName);
                appendLog(QString("Topic auto-set to %1 due to type selection").arg(topicName));
            }
        }
    };
    connect(cbType_, qOverload<int>(&QComboBox::currentIndexChanged), this, [populateFromCombo](int) { Q_UNUSED(populateFromCombo); populateFromCombo(); });
    connect(cbType_, qOverload<const QString&>(&QComboBox::currentTextChanged), this, [populateFromCombo](const QString&) { Q_UNUSED(populateFromCombo); populateFromCombo(); });
    connect(btnClearDds_, &QPushButton::clicked, this, &MainWindow::onClearDdsEntities);
    connect(btnClearLog_, &QPushButton::clicked, this, &MainWindow::onClearLog);
    connect(btnPayloadPopup_, &QPushButton::clicked, this, &MainWindow::onOpenPayloadPopup);

    QList<QPushButton*> btns = {btnConn_,   btnPart_,   btnPublisher_, btnSubscriber_, btnWriter_,
                                btnReader_, btnPub_,  btnClearDds_,  btnClearLog_};
    for (QPushButton* b : btns) {
        if (b) b->setStyleSheet(btnStyle);
    }

    statusBar()->showMessage("Ready");
}

void MainWindow::onOpenPayloadPopup()
{
    // Popup dialog that shows a large editable payload editor. Contents are synced when opening.
    QDialog dlg(this);
    dlg.setWindowTitle("Payload Editor");
    dlg.resize(800, 600);
    auto* lay = new QVBoxLayout(&dlg);
    auto* label = new QLabel(QString("Editing payload for type: %1").arg(cbType_->currentText()));
    label->setStyleSheet("font-weight:bold;padding:4px;");
    auto* editor = new QTextEdit;
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setPlainText(tePayload_->toPlainText());
    lay->addWidget(label);
    lay->addWidget(editor);
    auto* btns = new QHBoxLayout;
    auto* ok = new QPushButton("OK");
    auto* cancel = new QPushButton("Cancel");
    btns->addStretch();
    btns->addWidget(ok);
    btns->addWidget(cancel);
    lay->addLayout(btns);

    connect(ok, &QPushButton::clicked, &dlg, [&]() {
        tePayload_->setPlainText(editor->toPlainText());
        dlg.accept();
    });
    connect(cancel, &QPushButton::clicked, &dlg, [&]() { dlg.reject(); });

    dlg.exec();
}

void MainWindow::onClearLog()
{
    if (teLog_) teLog_->clear();
}

void MainWindow::updateUiState()
{
    // 연결 전에는 Participant/Writer/Reader/Publish 비활성
    const bool en = connected_;
    btnPart_->setEnabled(en);
    btnWriter_->setEnabled(en);
    btnReader_->setEnabled(en);
    btnPub_->setEnabled(en);

    // 연결 시 주소/포트/역할 잠금
    leRole_->setEnabled(!en);
    leAddr_->setEnabled(!en);
    lePort_->setEnabled(!en);

    // 버튼 라벨/색상
    btnConn_->setText(en ? "Disconnect" : "Connect");
    btnConn_->setStyleSheet(en ? "background:#2e7d32;color:white" : "");
}

void MainWindow::pulseButton(QWidget* w)
{
    if (!w) return;
    w->setProperty("pulse", true);
    w->setStyleSheet("QPushButton[pulse='true']{outline:2px solid #64b5f6;}");
    QTimer::singleShot(180, this, [w] {
        w->setProperty("pulse", false);
        w->setStyleSheet("");
    });
}

void MainWindow::appendLog(const QString& line, bool isDebug)
{
    if (isDebug && logLevel_ == LogLevel::Info) return;
    if (!teLog_) return;

    auto appendJsonBlock = [this](const QString& header, const QString& jsonText) {
        teLog_->append(header.toHtmlEscaped());
        // JSON pretty 영역을 monospace로 감싸서 가독성 향상
        const QString html = QString(
                                 "<pre style='margin:4px "
                                 "0;padding:6px;background:#1e1e1e;color:#dcdcdc;border-radius:4px;font-family:"
                                 "Consolas,monospace;'>%1</pre>")
                                 .arg(jsonText.toHtmlEscaped());
        teLog_->append(html);
        teLog_->moveCursor(QTextCursor::End);
    };

    // 특수 처리: SEND-REQ / RSP / EVT 내 JSON 블록을 예쁘게 출력
    if (line.startsWith("[SEND-REQ]")) {
        int p = line.indexOf(" json=");
        if (p > 0) {
            const QString header = line.left(p);
            const QString jsonText = line.mid(p + 6);
            appendJsonBlock(header, jsonText);
            return;
        }
    } else if (line.startsWith("[RSP]")) {
        int p = line.indexOf(" json=");
        if (p > 0) {
            const QString header = line.left(p);
            const QString jsonText = line.mid(p + 6);
            appendJsonBlock(header, jsonText);
            return;
        }
    } else if (line.startsWith("[EVT] ")) {
        // 형태: "[EVT] { ... }" → 접두부와 JSON 분리
        int p = line.indexOf('{');
        if (p > 0) {
            const QString header = line.left(p).trimmed();
            const QString jsonText = line.mid(p);
            appendJsonBlock(header, jsonText);
            return;
        }
    }

    // 일반 라인: 접두어별 색상 규칙
    // [WRN] : 빨강 (경고)
    // [ERR], [FTL] : 빨강, 굵게 (에러, 치명적)
    // [DBG] : 파랑 (디버그)
    // 기타 : 기본색
    if (line.startsWith("[WRN]"))
        teLog_->append(QString("<span style='color:#ff5252'>%1</span>").arg(line.toHtmlEscaped())); // 빨강
    else if (line.startsWith("[ERR]") || line.startsWith("[FTL]"))
        teLog_->append(QString("<span style='color:#ff5252;font-weight:bold'>%1</span>").arg(line.toHtmlEscaped())); // 빨강, 굵게
    else if (line.startsWith("[DBG]"))
        teLog_->append(QString("<span style='color:#90caf9'>%1</span>").arg(line.toHtmlEscaped())); // 파랑
    else
        teLog_->append(line.toHtmlEscaped()); // 기본색
    teLog_->moveCursor(QTextCursor::End);
}

void MainWindow::onLogLevelChanged(int index)
{
    logLevel_ = (index == 1) ? LogLevel::Debug : LogLevel::Info;
    statusBar()->showMessage(QString("LogLevel=%1").arg(index == 1 ? "Debug" : "Info"), 1000);
}

void MainWindow::onConnect()
{
    pulseButton(btnConn_);

    if (!connected_) {
        // Connect
        const QString roleStr = leRole_->text().trimmed().toLower();
        const Role role = (roleStr == "server") ? Role::Server : Role::Client;
        const std::string addr = leAddr_->text().toStdString();
        const uint16_t port = static_cast<uint16_t>(lePort_->text().toUShort());

        appendLog(QString("[UI] Connect role=%1 addr=%2 port=%3")
                      .arg(role == Role::Server ? "server" : "client")
                      .arg(leAddr_->text())
                      .arg(lePort_->text()));

        if (!ipc_.start(role, Endpoint{addr, port})) {
            appendLog("[ERR] ipc.start failed");
            statusBar()->showMessage("ipc.start failed", 2000);
            return;
        }
        connected_ = true;
        updateUiState();

        // 선택: 능력/버전 교환
        triad::rpc::RpcBuilder hello;
        hello.set_op("hello").proto(1);
        send_req(hello);
    } else {
        // Disconnect
        appendLog("[UI] Disconnect");
        ipc_.stop();
        connected_ = false;
        updateUiState();
    }
}

void MainWindow::onCreatePublisher()
{
    pulseButton(btnPublisher_);
    const int domain = leDomain_->text().toInt();
    const std::string pub_name = lePubName_->text().toStdString();
    const std::string qos = (leQosLib_->text() + "::" + leQosProf_->text()).toStdString();

    triad::rpc::RpcBuilder rb;
    rb.set_op("create")
        .set_target("publisher")
        .args({{"domain", domain}, {"publisher", pub_name}, {"qos", qos}})
        .proto(1);
    send_req(rb);
}

void MainWindow::onCreateSubscriber()
{
    pulseButton(btnSubscriber_);
    const int domain = leDomain_->text().toInt();
    const std::string sub_name = leSubName_->text().toStdString();
    const std::string qos = (leQosLib_->text() + "::" + leQosProf_->text()).toStdString();

    triad::rpc::RpcBuilder rb;
    rb.set_op("create")
        .set_target("subscriber")
        .args({{"domain", domain}, {"subscriber", sub_name}, {"qos", qos}})
        .proto(1);
    send_req(rb);
}

void MainWindow::onCreateParticipant()
{
    pulseButton(btnPart_);
    const int domain = leDomain_->text().toInt();
    const std::string qos = (leQosLib_->text() + "::" + leQosProf_->text()).toStdString();

    triad::rpc::RpcBuilder rb;
    rb.set_op("create").set_target("participant").args({{"domain", domain}, {"qos", qos}}).proto(1);
    send_req(rb);
}

void MainWindow::onCreateWriter()
{
    pulseButton(btnWriter_);
    const int domain = leDomain_->text().toInt();
    const std::string pub_name = lePubName_->text().toStdString();
    const std::string topic = leTopic_->text().toStdString();
    const std::string type = cbType_->currentData().toString().toStdString();
    const std::string qos = (leQosLib_->text() + "::" + leQosProf_->text()).toStdString();

    triad::rpc::RpcBuilder rb;
    rb.set_op("create")
        .set_target("writer", {{"topic", topic}, {"type", type}})
        .args({{"domain", domain}, {"publisher", pub_name}, {"qos", qos}})
        .proto(1);
    send_req(rb);
}

void MainWindow::onCreateReader()
{
    pulseButton(btnReader_);
    const int domain = leDomain_->text().toInt();
    const std::string sub_name = leSubName_->text().toStdString();
    const std::string topic = leTopic_->text().toStdString();
    const std::string type = cbType_->currentData().toString().toStdString();
    const std::string qos = (leQosLib_->text() + "::" + leQosProf_->text()).toStdString();

    triad::rpc::RpcBuilder rb;
    rb.set_op("create")
        .set_target("reader", {{"topic", topic}, {"type", type}})
        .args({{"domain", domain}, {"subscriber", sub_name}, {"qos", qos}})
        .proto(1);
    send_req(rb);
}

void MainWindow::onPublishSample()
{
    const QString qtype = cbType_->currentData().toString();
    const QString qtopic = leTopic_->text();

    // 1) JSON 유효성 검사
    const QByteArray raw = tePayload_->toPlainText().toUtf8();
    const QJsonParseError err{};
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        statusBar()->showMessage("Invalid JSON payload", 2000);
        return;
    }
    const QString jsonStr =
        QString::fromUtf8(QJsonDocument(doc.object()).toJson(QJsonDocument::Compact));

    triad::rpc::RpcBuilder rb;
    rb.set_op("write")
        .set_target("writer", {{"topic", qtopic.toStdString()}, {"type", qtype.toStdString()}})
        .data({{"text", jsonStr.toStdString()}})  // Gateway 계약: data.text = JSON 문자열
        .proto(1);

    send_req(rb);  // 기존 IPC 경로 그대로 사용
}



void MainWindow::onClearDdsEntities()
{
    pulseButton(btnClearDds_);
    triad::rpc::RpcBuilder rb;
    rb.set_op("clear").set_target("dds_entities").proto(1);
    send_req(rb);
}

// ------- 전송 헬퍼 --------
void MainWindow::send_cmd(uint16_t type, const void* payload, uint32_t len)
{
    ipc_.send_raw(type, ++corr_, reinterpret_cast<const uint8_t*>(payload), len);
}

void MainWindow::send_req(const triad::rpc::RpcBuilder& rb)
{
    auto cbor = rb.to_cbor();
    const bool ok =
        ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_REQ, ++corr_, cbor.data(), static_cast<uint32_t>(cbor.size()));
    appendLog(QString("[SEND-REQ] id=%1 json=%2").arg(corr_).arg(QString::fromStdString(rb.to_json(false))));
    if (!ok) statusBar()->showMessage("send_req failed", 1500);
}
