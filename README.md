# qt_virtualcan_silkit_bridge_windows

# TOOLS REQUIRED
CMAKE 4.3.2

Launch x64 Native Tools Command Prompt for VS 2019/2022
cd to repo folder
cmake -S . -B build -G Ninja  -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/msvc2022_64"

Configure project Cmake

cl.exe NO se usa desde el PATH normal del sistema.

Microsoft lo carga solo dentro de un entorno especial:

x64 Native Tools Command Prompt for VS 2019/2022

Ese terminal ejecuta un script (vcvars64.bat) que configura todo.

cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/msvc2022_64"

Compile
cmake --build  build -j

This is the official Qt solution.

Run:

C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe QtSilKitBridge.exe

👉 This automatically copies ALL required DLLs next to your executable

then run  QtSilKitBridge.exe

set SILKIT_BIN_PATH=C:/Program Files/Vector SIL Kit 5.0.4

pushd %SILKIT_BIN_PATH%

start ./sil-kit-registry.exe -u silkit://localhost:8500
start ./sil-kit-system-controller.exe --connect-uri silkit://localhost:8500 QtBridgeParticipant

popd


 ┌───────────────┐
        │   Qt Thread    │
        │ QCanBusDevice  │
        └──────┬────────┘
               │ signals (framesReceived)
               ▼
        ┌──────────────────┐
        │ Qt RX Queue      │  (thread-safe)
        └──────┬───────────┘
               ▼
        ┌──────────────────┐
        │ SIL Kit Thread   │
        │ CAN Controller   │
        └──────┬───────────┘
               │
               ▼
        ┌──────────────────┐
        │ SIL RX Queue     │
        └──────┬───────────┘
               ▼
        Qt Thread (writeFrame ONLY here)
