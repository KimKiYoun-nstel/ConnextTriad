# Domain Standards (NGVA/DDS/Connext)

## DDS/Connext
- Version pinned: RTI Connext DDS 7.5.x (Classic C++ API).
- Entities (Participant/Publisher/Subscriber/Topic/Writer/Reader) must be created via `dds_manager` only.
- QoS managed centrally with `qos/qos_profiles.xml` (copied next to the gateway exe).

## IDL & Generation Policy
- `.idl` files under `RtpDdsGateway/types/` are **authoritative**; commit to VCS.
- Generated **headers** may be read for data type reference.
- Generated **sources** are build artifacts; never edited or referenced.
- Schema changes: edit `.idl`, then regenerate.

## IPC Contract
- All UI interactions traverse `DkmRtpIpc` and `ipc_adapter` (ACK/ERR/EVT codes).
- Keep message formats stable; evolve via versioned envelopes if needed.

## NGVA Alignment
- Model topics/types according to NGVA data models where applicable.
- Avoid vendor-specific assumptions leaking into public interfaces.

## Versioning & Build
- `NDDSHOME` must be set; CMake auto-detects `RTI_LIB_DIR` (VS2017/2019/2022 variants) and links `nddscpp`, `nddsc`, `nddscore`.
- Define `USE_CONNEXT` to compile against Connext; otherwise stub behavior is enabled.
