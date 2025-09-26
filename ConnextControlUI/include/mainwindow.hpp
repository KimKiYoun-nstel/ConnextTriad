#pragma once
// UI 메인 윈도우: IPC 전송/수신, 상태표시, 로그 패널
#include <QComboBox>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <memory>

#include "dkmrtp_ipc.hpp"
#include "dkmrtp_ipc_messages.hpp"
#include "dkmrtp_ipc_types.hpp"
#include "rpc_envelope.hpp"  // A안: 빌더
#include "xml_type_catalog.hpp"

class MainWindow final : public QMainWindow
{
    Q_OBJECT
   public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    // 전역 로그 라우팅을 위한 메타 호출용(접근지정자 무관)
    Q_INVOKABLE void appendLog(const QString& line, bool isDebug = false);

   private slots:
    void onConnect();  // Connect/Disconnect 토글
    void onCreateParticipant();
    void onCreateWriter();
    void onCreateReader();
    void onPublishSample();
    void onLogLevelChanged(int index);
    void onCreatePublisher();
    void onCreateSubscriber();
    void onClearDdsEntities(); // DDS 엔티티 전체 초기화

   private:
    void setupUi();
    void updateUiState();
    void pulseButton(QWidget* w);
    void onTopicSelected(const QString& typeName);
    void updateTypeComboBoxes(); // 타입 콤보박스 업데이트

    // 레거시 전송(유지)
    void send_cmd(uint16_t type, const void* payload, uint32_t len);
    // 권장 경로: 빌더 기반 REQ 전송(CBOR)
    void send_req(const triad::rpc::RpcBuilder& rb);

   private:
    // 상태
    bool connected_{false};
    enum class LogLevel { Info, Debug } logLevel_{LogLevel::Info};
    uint32_t corr_{1};

    // XML 카탈로그 및 토픽 콤보
    XmlTypeCatalog catalog_;
    QComboBox* topicCombo_{nullptr};

    // 위젯
    QLineEdit* leRole_{nullptr};  // "client" / "server"
    QLineEdit* leAddr_{nullptr};  // ip/host
    QLineEdit* lePort_{nullptr};  // port
    QLineEdit* leDomain_{nullptr};
    QLineEdit* leQosLib_{nullptr};
    QLineEdit* leQosProf_{nullptr};
    QLineEdit* leTopic_{nullptr};
    QComboBox* cbType_{nullptr};
    QTextEdit* tePayload_{nullptr};
    QTextEdit* teLog_{nullptr};
    QLineEdit* lePubName_{nullptr};
    QLineEdit* leSubName_{nullptr};
    QPushButton* btnPublisher_{nullptr};
    QPushButton* btnSubscriber_{nullptr};

    QPushButton* btnConn_{nullptr};
    QPushButton* btnPart_{nullptr};
    QPushButton* btnWriter_{nullptr};
    QPushButton* btnReader_{nullptr};
    QPushButton* btnPub_{nullptr};
    QPushButton* btnClearDds_{nullptr}; // DDS 엔티티 전체 초기화 버튼
    QPushButton* btnClearLog_{nullptr}; // 로그 패널 clear 버튼
    QPushButton* btnOpenForm_{nullptr}; // Open Form 버튼
    void onClearLog(); // 로그 패널 clear 슬롯
    QComboBox* cbLogLevel_{nullptr};

    // IPC
    dkmrtp::ipc::DkmRtpIpc ipc_{};
};
