# Project Architecture (Proposed)

## Modules
- **RtpDdsGateway**
  - Static lib: `RtpDdsCore` + executable: `RtpDdsGateway`
  - Key components:
    - `dds_manager`: Owns DDS entities (Participant/Publisher/Subscriber/Topic/Writer/Reader) and routes samples via callbacks.
    - `ipc_adapter`: Translates UI IPC commands into DDS operations (ACK/ERR/EVT back to UI).
    - `data_converter`: Converts between text/JSON and DDS types (specialized per IDL type).
    - `type_registry`: Central registry mapping type names → factory & helpers (type name, register_type, writer/reader creators, I/O helpers).
  - Consumes IDL-defined types under `RtpDdsGateway/types/*.idl` (e.g., `AlarmMsg.idl`, `StringMsg.idl`).

- **ConnextControlUI**
  - GUI app (Qt). Sends RPC/commands via IPC and renders logs/events.
  - Uses `rpc_envelope` builder to structure requests (`to_cbor()`, `to_json()`).

- **DkmRtpIpc**
  - Lightweight UDP IPC (client/server) with background recv loop and callbacks.
  - Provides framed messages and helpers to send raw/evt/error.

- **QoS**
  - `qos/qos_profiles.xml` copied next to the gateway executable at build/post-build.

## Build & Generation
- **IDL → Generated code**: `rtiddsgen` creates Classic C++ outputs into `${binary_dir}/generated`.
- Generated headers (`*.h`) supply the concrete data types. Generated sources (`*.cxx`, `*Plugin.*`, `*Support.*`) are build artifacts and not edited.
- CMake auto-detects RTI lib dir (prefers `x64Win64VS2017/2019/2022`) and links `nddscpp`, `nddsc`, `nddscore`. `NDDSHOME` is required.

## Data Flow (Runtime)
```
UI (ConnextControlUI)
   ⇅  DkmRtpIpc (UDP, request/response, events)
   ⇅  ipc_adapter (command translation + ack/err/evt)
   ⇅  dds_manager (manage readers/writers + callbacks)
   ⇅  RTI Connext DDS network (topics from IDL)
```
- Incoming DDS samples trigger `ReaderListener::on_data_available` → converted via `data_converter` → surfaced to UI via IPC.
- Outgoing UI commands translated to create entities, write samples, subscribe, etc.

## Concurrency & Ownership
- `DkmRtpIpc` runs a recv thread; callbacks are marshaled to the app layer.
- `dds_manager` owns DDS entity maps keyed by domain/topic (participants_, publishers_, subscribers_, writers_, readers_).
- `ReaderListener` is per-topic; avoids heavy work inside listener; use converter/IPC pipeline.

## Extension Workflows
### Add a new DDS topic type
1. Add `types/NewType.idl` (commit to VCS).
2. Reconfigure/generate via CMake/rtiddsgen → headers appear under `generated/`.
3. Add `DataConverter<NewType>` specialization.
4. Register in `TypeRegistry` (name, register_type, writer/reader creators, publish/take helpers).
5. Use in `dds_manager` ops and UI RPC bindings.
6. Do **not** hand-edit generated sources.

### Add a new UI command
1. Define the RPC envelope in UI (`rpc_envelope`).
2. Handle in `ipc_adapter` → translate to appropriate `dds_manager` action.
3. Reuse existing functions where possible; avoid duplication.

## Boundaries (Do/Don't)
- **Do**: Only `dds_manager` creates DDS entities; UI never accesses DDS directly.
- **Do**: Centralize type transforms in `data_converter`; do not spread conversions.
- **Don't**: Modify files in `generated/` except headers for read-only reference; schema changes live in `.idl`.
- **Don't**: Duplicate similar logic; check `type_registry`, `ipc_adapter`, `dds_manager` first.

## Observability & Logging (Guidance)
- Use existing IPC/log hooks in `DkmRtpIpc` & UI log panel.
- For DDS events, prefer structured messages (JSON) when routing to UI.
