"""
IPC Protocol Module
Extracts packet header and CBOR logic from test_runner.py
"""
import struct
import time
import cbor2

# RIPC-like 헤더 포맷 (프로젝트 문서와 호환되도록 설계)
MAGIC = 0x52495043  # 'RIPC'
VERSION = 0x0001
MSG_FRAME_REQ = 0x1000
MSG_FRAME_RSP = 0x1001
MSG_FRAME_EVT = 0x1002

# struct format: magic(4) ver(2) type(2) corr_id(4) length(4) ts_ns(8)
HEADER_FMT = "!I H H I I Q"
HEADER_LEN = struct.calcsize(HEADER_FMT)


def now_ns() -> int:
    return time.time_ns()


def pack_frame(payload_bytes: bytes, frame_type: int, corr_id: int) -> bytes:
    length = len(payload_bytes)
    header = struct.pack(HEADER_FMT, MAGIC, VERSION, frame_type, corr_id, length, now_ns())
    return header + payload_bytes


def unpack_header(buf: bytes) -> dict:
    if len(buf) < HEADER_LEN:
        raise ValueError("buffer too small for header")
    vals = struct.unpack(HEADER_FMT, buf[:HEADER_LEN])
    return {
        "magic": vals[0],
        "version": vals[1],
        "type": vals[2],
        "corr_id": vals[3],
        "length": vals[4],
        "ts_ns": vals[5],
    }
