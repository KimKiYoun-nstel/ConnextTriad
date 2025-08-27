#include "mainwindow.hpp"
#include <QtWidgets>
#include <nlohmann/json.hpp> // 수신 CBOR → JSON pretty 로그에 사용

using dkmrtp::ipc::Endpoint;
using dkmrtp::ipc::Header;
using dkmrtp::ipc::Role;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();

    // IPC 콜백 설치(레거시 + 새 프레임)
    dkmrtp::ipc::DkmRtpIpc::Callbacks cb{};

    // 레거시 경로(겸용 유지)
        // 불필요한 콜백 제거됨. REQ/RSP/EVT만 남김.

    // 새 경로(REQ/RSP/EVT = CBOR 페이로드)
    cb.on_response = [this](const Header &h, const uint8_t *body,
                            uint32_t len) {
        try {
            auto j = nlohmann::json::from_cbor(
                std::vector<uint8_t>(body, body + len));
            appendLog(QString("[RSP] id=%1 json=%2")
                          .arg(h.corr_id)
                          .arg(QString::fromStdString(j.dump(2))));
            statusBar()->showMessage(
                QString("RSP id=%1 ok=%2")
                    .arg(h.corr_id)
                    .arg(j.value("ok", false) ? "true" : "false"),
                2000);
        } catch (...) {
            appendLog(QString("[RSP] id=%1 <CBOR parse error>").arg(h.corr_id));
        }
    };
    cb.on_event = [this](const Header &, const uint8_t *body, uint32_t len) {
        try {
            auto j = nlohmann::json::from_cbor(
                std::vector<uint8_t>(body, body + len));
            appendLog(
                QString("[EVT] %1").arg(QString::fromStdString(j.dump(2))));
        } catch (...) {
            appendLog("[EVT] <CBOR parse error>");
        }
    };

    ipc_.set_callbacks(cb);
    updateUiState();
}

void MainWindow::setupUi() {
    setWindowTitle("Connext Control UI");
    resize(900, 560);

    auto *w = new QWidget;
    setCentralWidget(w);
    auto *lay = new QVBoxLayout(w);

    // 연결 영역
    auto *gbConn = new QGroupBox("Connection");
    auto *cl = new QHBoxLayout(gbConn);
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

    // Participant
    auto *gbPart = new QGroupBox("Participant");
    auto *pl = new QHBoxLayout(gbPart);
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
    auto *gbIO = new QGroupBox("Pub/Sub");
    auto *il = new QGridLayout(gbIO);
    leTopic_ = new QLineEdit("HelloTopic");
    cbType_ = new QComboBox();
    cbType_->setEditable(true);
    cbType_->addItems({"StringMsg", "AlarmMsg"});
    tePayload_ = new QTextEdit("Hello from UI");
    btnWriter_ = new QPushButton("Create Writer");
    btnReader_ = new QPushButton("Create Reader");
    btnPub_ = new QPushButton("Publish");
    int r = 0;
    il->addWidget(new QLabel("topic"), r, 0);
    il->addWidget(leTopic_, r, 1);
    r++;
    il->addWidget(new QLabel("type"), r, 0);
    il->addWidget(cbType_, r, 1);
    r++;
    il->addWidget(btnWriter_, r, 0);
    il->addWidget(btnReader_, r, 1);
    r++;
    il->addWidget(new QLabel("payload"), r, 0);
    il->addWidget(tePayload_, r, 1);
    r++;
    il->addWidget(btnPub_, r, 1);
    lay->addWidget(gbIO);

    // Log level + 로그 패널
    auto *ll = new QHBoxLayout();
    cbLogLevel_ = new QComboBox();
    cbLogLevel_->addItems({"Info", "Debug"});
    ll->addWidget(new QLabel("Log Level"));
    ll->addWidget(cbLogLevel_);
    ll->addStretch();
    lay->addLayout(ll);

    auto *gbLog = new QGroupBox("Log");
    auto *lg = new QVBoxLayout(gbLog);
    teLog_ = new QTextEdit();
    teLog_->setReadOnly(true);
    teLog_->setMinimumHeight(160);
    lg->addWidget(teLog_);
    lay->addWidget(gbLog);

    // 연결
    connect(btnConn_, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(btnPart_, &QPushButton::clicked, this,
            &MainWindow::onCreateParticipant);
    connect(btnWriter_, &QPushButton::clicked, this,
            &MainWindow::onCreateWriter);
    connect(btnReader_, &QPushButton::clicked, this,
            &MainWindow::onCreateReader);
    connect(btnPub_, &QPushButton::clicked, this, &MainWindow::onPublishSample);
    connect(cbLogLevel_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &MainWindow::onLogLevelChanged);

    statusBar()->showMessage("Ready");
}

void MainWindow::updateUiState() {
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

void MainWindow::pulseButton(QWidget *w) {
    if (!w)
        return;
    w->setProperty("pulse", true);
    w->setStyleSheet("QPushButton[pulse='true']{outline:2px solid #64b5f6;}");
    QTimer::singleShot(180, this, [w] {
        w->setProperty("pulse", false);
        w->setStyleSheet("");
    });
}

void MainWindow::appendLog(const QString &line, bool isDebug) {
    if (isDebug && logLevel_ == LogLevel::Info)
        return;
    if (!teLog_) return;

    auto appendJsonBlock = [this](const QString &header, const QString &jsonText) {
        teLog_->append(header.toHtmlEscaped());
        // JSON pretty 영역을 monospace로 감싸서 가독성 향상
        const QString html = QString(
                                 "<pre style='margin:4px 0;padding:6px;background:#1e1e1e;color:#dcdcdc;border-radius:4px;font-family:Consolas,monospace;'>%1</pre>")
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

    // 일반 라인: 기존 색상 규칙 유지
    if (line.startsWith("[WRN]"))
        teLog_->append(QString("<span style='color:#ffb300'>%1</span>")
                           .arg(line.toHtmlEscaped()));
    else if (line.startsWith("[ERR]") || line.startsWith("[FTL]"))
        teLog_->append(QString("<span style='color:#ff5252'>%1</span>")
                           .arg(line.toHtmlEscaped()));
    else if (line.startsWith("[DBG]"))
        teLog_->append(QString("<span style='color:#90caf9'>%1</span>")
                           .arg(line.toHtmlEscaped()));
    else
        teLog_->append(line.toHtmlEscaped());
    teLog_->moveCursor(QTextCursor::End);
}

void MainWindow::onLogLevelChanged(int index) {
    logLevel_ = (index == 1) ? LogLevel::Debug : LogLevel::Info;
    statusBar()->showMessage(
        QString("LogLevel=%1").arg(index == 1 ? "Debug" : "Info"), 1000);
}

void MainWindow::onConnect() {
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

void MainWindow::onCreateParticipant() {
    pulseButton(btnPart_);
    const int domain = leDomain_->text().toInt();
    const std::string qos =
        (leQosLib_->text() + "::" + leQosProf_->text()).toStdString();

    triad::rpc::RpcBuilder rb;
    rb.set_op("create")
        .set_target("participant")
        .args({{"domain", domain}, {"qos", qos}})
        .proto(1);
    send_req(rb);
}

void MainWindow::onCreateWriter() {
    pulseButton(btnWriter_);
    const std::string topic = leTopic_->text().toStdString();
    const std::string type = cbType_->currentText().toStdString();
    const std::string qos =
        (leQosLib_->text() + "::" + leQosProf_->text()).toStdString();

    triad::rpc::RpcBuilder rb;
    rb.set_op("create")
        .set_target("writer", {{"topic", topic}, {"type", type}})
        .args({{"qos", qos}})
        .proto(1);
    send_req(rb);
}

void MainWindow::onCreateReader() {
    pulseButton(btnReader_);
    const std::string topic = leTopic_->text().toStdString();
    const std::string type = cbType_->currentText().toStdString();
    const std::string qos =
        (leQosLib_->text() + "::" + leQosProf_->text()).toStdString();

    triad::rpc::RpcBuilder rb;
    rb.set_op("create")
        .set_target("reader", {{"topic", topic}, {"type", type}})
        .args({{"qos", qos}})
        .proto(1);
    send_req(rb);
}

void MainWindow::onPublishSample() {
    pulseButton(btnPub_);
    const std::string topic = leTopic_->text().toStdString();
    const std::string type = cbType_->currentText().toStdString();
    const std::string text = tePayload_->toPlainText().toStdString();

    triad::rpc::RpcBuilder rb;
    rb.set_op("write")
        .set_target("writer", {{"topic", topic}, {"type", type}})
        .data({{"text", text}})
        .proto(1);
    send_req(rb);
}

// ------- 전송 헬퍼 --------
void MainWindow::send_cmd(uint16_t type, const void *payload, uint32_t len) {
    ipc_.send_raw(type, ++corr_, reinterpret_cast<const uint8_t *>(payload),
                  len);
}

void MainWindow::send_req(const triad::rpc::RpcBuilder &rb) {
    auto cbor = rb.to_cbor();
    const bool ok =
        ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_REQ, ++corr_, cbor.data(),
                        static_cast<uint32_t>(cbor.size()));
    appendLog(QString("[SEND-REQ] id=%1 json=%2")
                  .arg(corr_)
                  .arg(QString::fromStdString(rb.to_json(false))));
    if (!ok)
        statusBar()->showMessage("send_req failed", 1500);
}
