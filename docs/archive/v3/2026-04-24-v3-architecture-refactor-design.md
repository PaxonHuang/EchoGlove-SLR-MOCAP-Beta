---
name: V3 Architecture Refactor
description: Refactor project to V3 framework - remove Tauri/Rust, integrate glove_firmware/glove_relay/glove_web/glove_unity
type: project
---

# V3 Architecture Refactor Design Document

**Date:** 2026-04-24  
**Status:** Approved  
**Scope:** Full project restructuring

---

## 1. Project Context

### Current State
- Repository contains V2/V7 legacy code with Tauri/Rust references
- Existing firmware: `src/main.cpp`, `lib/Sensors/*`, `lib/Filters/*`
- New V3 framework in: `GLM-V10_CodeFramework/EdgeAI_DataGlove_V3_CodeFramework/`

### Target State
- Remove all Tauri/Rust/Cargo references
- Integrate V3 four-subsystem architecture:
  - `glove_firmware/` (ESP32 PlatformIO)
  - `glove_relay/` (Python FastAPI)
  - `glove_web/` (React + R3F)
  - `glove_unity/` (Unity 2022 LTS skeleton)

### Key Decisions (User Approved)
1. **激进重构**: Delete Tauri/Rust + clean redundant files + restructure directories
2. **保留Unity骨架**: Unity as L3 professional path, not in main flow
3. **升级到V3框架**: Firmware uses V3 code with ModelRegistry and static_assert
4. **先复制后清理**: Copy V3 framework first, then clean old code
5. **删除旧版文档**: Only keep V3 SOP and Prompts

---

## 2. Architecture

### Target Directory Structure

```
H:\HandSignRecognition\Alpha\Hall-BNO085-PlatformIOArduino\
├── CLAUDE.md                  ← Update to V3 version
├── README.md                  ← Update project overview
├── docs/
│   ├── SOP_SPEC_PLAN_V3.md    ← Main SOP (from V3)
│   ├── CLAUDE_CODE_PROMPTS_V3.md ← 28 Prompts (from V3)
│   └── references/            ← Chip manuals, papers (from V3)
│
├── glove_firmware/            ← ESP32 firmware (from V3)
│   ├── platformio.ini
│   ├── src/main.cpp           ← With static_assert fix
│   ├── lib/
│   │   ├── Sensors/           ← SensorManager, TCA, TMAG, BNO085
│   │   ├── Filters/           ← KalmanFilter1D
│   │   ├── Comms/             ← BLE, UDP, Protobuf
│   │   └── Models/            ← BaseModel, ModelRegistry
│   ├── include/
│   └── spiffs/
│
├── glove_relay/               ← Python relay (from V3)
│   ├── src/
│   ├── configs/
│   ├── scripts/
│   └── tests/
│
├── glove_web/                 ← React frontend (from V3)
│   ├── src/
│   ├── public/
│   └── package.json
│
├── glove_unity/               ← Unity skeleton (from V3)
│   └── Assets/Scripts/
│
└── archive/                   ← Temporary (will be deleted)
    ├── src_old/
    ├── lib_old/
    └── GLM-V7-old/
```

---

## 3. Components

### 3.1 glove_firmware (ESP32-S3)

**Why:** V3 firmware has:
- Fixed FreeRTOS xTaskCreatePinnedToCore bug (static_assert validation)
- ModelRegistry for hot-switching
- Unified data structures in `include/data_structures.h`

**Integration:**
- Copy `GLM-V10_CodeFramework/EdgeAI_DataGlove_V3_CodeFramework/glove_firmware/` → `glove_firmware/`
- Remove old `src/main.cpp` and `lib/*` after backup

### 3.2 glove_relay (Python)

**Why:** Unified relay architecture for:
- UDP receive (port 8888) + Protobuf parsing
- WebSocket push (port 8765) + JSON
- L2 ST-GCN inference
- NLP grammar correction + TTS

**Integration:**
- Copy `GLM-V10_CodeFramework/EdgeAI_DataGlove_V3_CodeFramework/glove_relay/` → `glove_relay/`

### 3.3 glove_web (React)

**Why:** Pure web frontend replacing Tauri/Rust:
- React 18 + Vite + R3F for 3D hand skeleton
- Zustand for state management
- WebSocket hook with auto-reconnect
- PWA support (optional)

**Integration:**
- Copy `GLM-V10_CodeFramework/EdgeAI_DataGlove_V3_CodeFramework/glove_web/` → `glove_web/`

### 3.4 glove_unity (Skeleton)

**Why:** L3 professional rendering path:
- Unity 2022 LTS + XR Hands + ms-MANO
- Not in main MVP flow, kept as skeleton

**Integration:**
- Copy `GLM-V10_CodeFramework/EdgeAI_DataGlove_V3_CodeFramework/glove_unity/` → `glove_unity/`

### 3.5 docs/

**Why:** V3 documentation is complete:
- SOP_SPEC_PLAN_V3.md (938 lines) - full phase plan
- CLAUDE_CODE_PROMPTS_V3.md (28 prompts) - executable guides
- references/ - chip manuals and papers

**Integration:**
- Copy `GLM-V10_CodeFramework/EdgeAI_DataGlove_V3_CodeFramework/docs/` → `docs/`
- Delete `GLM-V7-SOP-en/EdgeAI_DataGlove_Deliverables/`

---

## 4. Data Flow

```
ESP32-S3 (glove_firmware)     Python Relay (glove_relay)     Frontend (glove_web)
┌─────────────────┐           ┌──────────────────┐          ┌─────────────────┐
│ TMAG5273 ×5     │  UDP:8888 │ FastAPI Server   │ WS:8765  │ React + R3F     │
│ BNO085 ×1       │──Proto──→│ UDP Receiver     │──JSON──→│ 3D Hand Skeleton│
│ TCA9548A        │  (100Hz)  │ Protobuf Parser  │ (100Hz)  │ Zustand Store   │
│                 │           │ L2 ST-GCN        │          │ TailwindCSS UI  │
│ FreeRTOS        │           │ NLP + TTS        │          │ PWA             │
│ Core1: 100Hz    │           │                  │          │                 │
│ Core0: L1+Comm  │           └──────────────────┘          └─────────────────┘
└─────────────────┘
```

---

## 5. Delete List

### Must Delete (Tauri/Rust)
- Any `Cargo.toml` files
- Any `.rs` Rust source files
- Any `tauri.conf.json` files
- Any `cargo` references in documentation

### Must Delete (V7 Legacy)
- `GLM-V7-SOP-en.EdgeAI_DataGlove_Full_Deliverables/` directory
- `GLM-V10_CodeFramework/` after integration (source copy)
- Any `.zip` archive files

### Archive Then Delete (Old Firmware)
- Current `src/main.cpp` → `archive/src_old/`
- Current `lib/Sensors/*` → `archive/lib_old/`
- Current `lib/Filters/*` → `archive/lib_old/`

---

## 6. Verification

### Success Criteria
1. No `.rs`, `Cargo.toml`, `tauri.conf.json` in repository
2. `glove_firmware/platformio.ini` has PSRAM config
3. `glove_firmware/src/main.cpp` has `static_assert` validation
4. `glove_relay/src/main.py` exists with FastAPI
5. `glove_web/package.json` has React + R3F dependencies
6. `docs/SOP_SPEC_PLAN_V3.md` exists
7. `pio run` in glove_firmware compiles successfully

### Post-Integration Tests
1. `cd glove_firmware && pio run` - firmware compiles
2. `cd glove_web && npm install && npm run build` - frontend builds
3. `cd glove_relay && pip install -r requirements.txt` - relay deps install

---

## 7. Implementation Phases

### Phase A: Backup & Prepare
1. Create `archive/` directory
2. Move old `src/` and `lib/` to archive

### Phase B: Copy V3 Components
1. Copy `glove_firmware/` from V3 framework
2. Copy `glove_relay/` from V3 framework
3. Copy `glove_web/` from V3 framework
4. Copy `glove_unity/` from V3 framework
5. Copy `docs/` from V3 framework

### Phase C: Clean Old Files
1. Delete `GLM-V7-SOP-en/` directory
2. Delete any ZIP files
3. Delete `GLM-V10_CodeFramework/` (source)
4. Delete archive after verification

### Phase D: Update Root Files
1. Update `CLAUDE.md` to V3 version
2. Update `README.md` to V3 version
3. Update `.gitignore` for V3 structure

### Phase E: Verify
1. Check for Rust references (grep)
2. Compile firmware
3. Install relay dependencies
4. Build frontend

---

**Why:** This design ensures complete migration to V3 architecture with no Rust/Tauri legacy, proper backup of working code, and clear verification steps.

**How to apply:** Execute phases A-E sequentially, verify after each phase.