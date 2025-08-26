// =========================
// File: DkmRtpIpc/src/dkmrtp_ipc.cpp
// =========================
#include "dkmrtp_ipc.hpp"
#include <chrono>
#include <cstring>

using namespace dkmrtp::ipc;

// ---- 정적 유틸 ------------------------------------------------------------
PeerId DkmRtpIpc::make_peer_id(const sockaddr_in &a) {
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, (void *)&a.sin_addr, ip, sizeof(ip));
    char out[64] = {0};
    _snprintf_s(out, sizeof(out), _TRUNCATE, "%s:%u", ip,
                (unsigned)ntohs(a.sin_port));
    return PeerId(out);
}

uint64_t DkmRtpIpc::now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
        .count();
}

// ---- 수명주기 -------------------------------------------------------------
DkmRtpIpc::DkmRtpIpc() {
}
DkmRtpIpc::~DkmRtpIpc() {
    stop();
}

bool DkmRtpIpc::open_socket(Role role, const Endpoint &ep) {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;
#endif
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    // 바인드/커넥트
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ep.address.c_str(), &addr.sin_addr);
    addr.sin_port = htons(ep.port);

    if (role == Role::Server) {
        if (::bind(s, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            ::closesocket(s);
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }
    } else {
        if (::connect(s, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            ::closesocket(s);
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }
    }

    sock_ = new SOCKET(s);
    role_ = role;
    ep_ = ep;
    return true;
}

void DkmRtpIpc::close_socket() {
    if (!sock_)
        return;
    SOCKET s = *reinterpret_cast<SOCKET *>(sock_);
    ::closesocket(s);
    delete reinterpret_cast<SOCKET *>(sock_);
    sock_ = nullptr;
#ifdef _WIN32
    WSACleanup();
#endif
}

bool DkmRtpIpc::start(Role role, const Endpoint &ep) {
    if (running_)
        return true;
    if (!open_socket(role, ep))
        return false;
    running_ = true;
    rx_thread_ = std::thread([this] { recv_loop(); });
    return true;
}

void DkmRtpIpc::stop() {
    if (!running_)
        return;
    running_ = false;
    if (sock_) {
        // 깨우기용 더미 send (Windows에서는 select 타임아웃으로 빠짐)
    }
    if (rx_thread_.joinable())
        rx_thread_.join();
    close_socket();
}

// ---- 콜백 등록 ------------------------------------------------------------
void DkmRtpIpc::set_callbacks(const Callbacks &cb) {
    cb_ = cb;
}
void DkmRtpIpc::set_callbacks_ex(const CallbacksEx &cb) {
    cb_ex_ = cb;
}

// ---- Peer 목록/조작 -------------------------------------------------------
std::vector<PeerId> DkmRtpIpc::list_peers() const {
    std::lock_guard<std::mutex> lk(peers_mtx_);
    std::vector<PeerId> v;
    v.reserve(peers_.size());
    for (auto &kv : peers_)
        if (kv.second.valid)
            v.push_back(kv.first);
    return v;
}
bool DkmRtpIpc::is_peer_valid(const PeerId &id) const {
    std::lock_guard<std::mutex> lk(peers_mtx_);
    auto it = peers_.find(id);
    return (it != peers_.end() && it->second.valid);
}
bool DkmRtpIpc::remove_peer(const PeerId &id) {
    std::lock_guard<std::mutex> lk(peers_mtx_);
    return peers_.erase(id) > 0;
}

// ---- 전송 ---------------------------------------------------------------
bool DkmRtpIpc::send_raw(uint16_t type, uint32_t corr_id,
                         const uint8_t *payload, uint32_t len) {
    if (!sock_)
        return false;
    if (len && !payload)
        return false;

    Header raw{};
    raw.magic = htonl(0x52495043); // 'RIPC'
    raw.version = htons(0x0001);
    raw.type = htons(type);
    raw.corr_id = htonl(corr_id);
    raw.length = htonl(len);
    raw.ts_ns = htonll(now_ns());

    std::lock_guard<std::mutex> lk(send_mtx_);
    SOCKET s = *reinterpret_cast<SOCKET *>(sock_);

    WSABUF bufs[2];
    bufs[0].buf = reinterpret_cast<char *>(&raw);
    bufs[0].len = sizeof(raw);
    bufs[1].buf = (char *)payload;
    bufs[1].len = len;

    DWORD sent = 0;
    int rc = 0;

    if (role_ == Role::Client) {
        rc = WSASend(s, bufs, 2, &sent, 0, nullptr, nullptr);
        return (rc == 0 && sent == sizeof(raw) + len);
    } else {
        // 서버에서는 대상이 명확해야 함 → send_raw_to() 사용 권장
        return false;
    }
}

bool DkmRtpIpc::send_raw_to(const PeerId &id, uint16_t type, uint32_t corr_id,
                            const uint8_t *payload, uint32_t len) {
    if (!sock_)
        return false;
    if (len && !payload)
        return false;

    IpcPeer dst{};
    {
        std::lock_guard<std::mutex> lk(peers_mtx_);
        auto it = peers_.find(id);
        if (it == peers_.end() || !it->second.valid)
            return false;
        dst = it->second;
    }

    Header raw{};
    raw.magic = htonl(0x52495043);
    raw.version = htons(0x0001);
    raw.type = htons(type);
    raw.corr_id = htonl(corr_id);
    raw.length = htonl(len);
    raw.ts_ns = htonll(now_ns());

    std::lock_guard<std::mutex> lk(send_mtx_);
    SOCKET s = *reinterpret_cast<SOCKET *>(sock_);

    WSABUF bufs[2];
    bufs[0].buf = reinterpret_cast<char *>(&raw);
    bufs[0].len = sizeof(raw);
    bufs[1].buf = (char *)payload;
    bufs[1].len = len;

    DWORD sent = 0;
    int rc = WSASendTo(s, bufs, 2, &sent, 0, (const sockaddr *)&dst.addr,
                       sizeof(dst.addr), nullptr, nullptr);
    return (rc == 0 && sent == sizeof(raw) + len);
}

bool DkmRtpIpc::broadcast_raw(uint16_t type, uint32_t corr_id,
                              const uint8_t *payload, uint32_t len) {
    auto list = list_peers();
    bool ok = true;
    for (auto &id : list)
        ok &= send_raw_to(id, type, corr_id, payload, len);
    return ok;
}

bool DkmRtpIpc::send_frame(uint16_t frame_type, uint32_t corr_id,
                           const uint8_t *payload, uint32_t len) {
    return send_raw(frame_type, corr_id, payload, len);
}

bool DkmRtpIpc::send_ack(uint32_t corr_id) {
    return send_raw(MSG_RSP_ACK, corr_id, nullptr, 0);
}

bool DkmRtpIpc::send_error(uint32_t corr_id, uint32_t code, const char *msg) {
    RspError e{};
    e.err_code = code;
    const uint8_t *m = reinterpret_cast<const uint8_t *>(msg ? msg : "");
    WSABUF bufs[2];

    Header raw{};
    raw.magic = htonl(0x52495043);
    raw.version = htons(0x0001);
    raw.type = htons(MSG_RSP_ERROR);
    raw.corr_id = htonl(corr_id);
    raw.length =
        htonl((uint32_t)sizeof(RspError) + (uint32_t)strlen((const char *)m));
    raw.ts_ns = htonll(now_ns());

    std::lock_guard<std::mutex> lk(send_mtx_);
    SOCKET s = *reinterpret_cast<SOCKET *>(sock_);

    bufs[0].buf = reinterpret_cast<char *>(&raw);
    bufs[0].len = sizeof(raw);
    bufs[1].buf = reinterpret_cast<char *>(
        &e); // 뒤에 문자열은 별도 전송하지 않고, 상위가 body를 그대로
             // 해석하도록 단순화 가능
    bufs[1].len = sizeof(RspError);

    DWORD sent = 0;
    int rc;
    if (role_ == Role::Client)
        rc = WSASend(s, bufs, 2, &sent, 0, nullptr, nullptr);
    else
        return false; // 서버는 send_raw_to 사용 권장

    return (rc == 0 && sent == bufs[0].len + bufs[1].len);
}

bool DkmRtpIpc::send_evt_data(const char *topic, const uint8_t *data,
                              uint32_t len, uint32_t corr_id) {
    // 게이트웨이가 서버일 때는 topic/data를 상위에서 CBOR로 패킹 후
    // send_raw_to/broadcast_raw 사용 권장. 여기서는 간단히 MSG_EVT_DATA로
    // 클라이언트 커넥션에 전송만 지원.
    if (role_ != Role::Client)
        return false;
    return send_raw(MSG_EVT_DATA, corr_id, data, len);
}

// ---- 수신 루프 -----------------------------------------------------------
void DkmRtpIpc::recv_loop() {
    SOCKET s = *reinterpret_cast<SOCKET *>(sock_);
    std::vector<uint8_t> buf(64 * 1024);

    while (running_) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);
        timeval tv{1, 0};
        int sel = select(0, &rfds, nullptr, nullptr, &tv);
        if (sel <= 0)
            continue;

        sockaddr_in peer{};
        int peer_len = sizeof(peer);
        int recvd = 0;

        if (role_ == Role::Server) {
            recvd = recvfrom(s, (char *)buf.data(), (int)buf.size(), 0,
                             (sockaddr *)&peer, &peer_len);
            if (recvd <= (int)sizeof(Header))
                continue;
            // peer 등록/갱신
            {
                std::lock_guard<std::mutex> lk(peers_mtx_);
                auto &slot = peers_[make_peer_id(peer)];
                slot.addr = peer;
                slot.valid = true;
                slot.last_active_ns = now_ns();
            }
        } else {
            recvd = recv(s, (char *)buf.data(), (int)buf.size(), 0);
            if (recvd <= (int)sizeof(Header))
                continue;
            // 클라이언트는 서버 1개를 peers_로 유지(선택)
            sockaddr_in svr{};
            svr.sin_family = AF_INET;
            svr.sin_port = htons(ep_.port);
            inet_pton(AF_INET, ep_.address.c_str(), &svr.sin_addr);
            {
                std::lock_guard<std::mutex> lk(peers_mtx_);
                auto &slot = peers_[make_peer_id(svr)];
                slot.addr = svr;
                slot.valid = true;
                slot.last_active_ns = now_ns();
            }
        }

        // 헤더 파싱
        Header wire{};
        memcpy(&wire, buf.data(), sizeof(wire));
        Header h{};
        h.magic = ntohl(wire.magic);
        h.version = ntohs(wire.version);
        h.type = ntohs(wire.type);
        h.corr_id = ntohl(wire.corr_id);
        h.length = ntohl(wire.length);
        h.ts_ns = ntohll(wire.ts_ns);

        const uint8_t *payload = buf.data() + sizeof(Header);
        uint32_t plen = (uint32_t)(recvd - (int)sizeof(Header));

        // 수신 검증
        if (h.magic != 0x52495043)
            continue; // 'RIPC'
        if (h.version != 0x0001)
            continue;
        if (h.length != plen)
            continue;

        // 디스패치
        switch (h.type) {
        case MSG_FRAME_REQ:
            if (cb_ex_.on_request)
                cb_ex_.on_request("", h, payload, plen);
            else if (cb_.on_request)
                cb_.on_request(h, payload, plen);
            else if (cb_.on_unhandled)
                cb_.on_unhandled(h);
            break;
        case MSG_FRAME_RSP:
            if (cb_ex_.on_response)
                cb_ex_.on_response("", h, payload, plen);
            else if (cb_.on_response)
                cb_.on_response(h, payload, plen);
            else if (cb_.on_unhandled)
                cb_.on_unhandled(h);
            break;
        case MSG_FRAME_EVT:
            if (cb_ex_.on_event)
                cb_ex_.on_event("", h, payload, plen);
            else if (cb_.on_event)
                cb_.on_event(h, payload, plen);
            else if (cb_.on_unhandled)
                cb_.on_unhandled(h);
            break;
        case MSG_EVT_DATA:
            if (cb_.on_evt_data)
                cb_.on_evt_data(h, payload);
            else if (cb_.on_unhandled)
                cb_.on_unhandled(h);
            break;
        case MSG_RSP_ACK:
            if (cb_.on_ack)
                cb_.on_ack(h);
            else if (cb_.on_unhandled)
                cb_.on_unhandled(h);
            break;
        case MSG_RSP_ERROR: {
            if (cb_.on_error) {
                RspError e{};
                memcpy(&e, payload, std::min<uint32_t>(sizeof(e), plen));
                cb_.on_error(h, e, "");
            } else if (cb_.on_unhandled)
                cb_.on_unhandled(h);
            break;
        }
        default:
            if (cb_.on_unhandled)
                cb_.on_unhandled(h);
            break;
        }
    }
}