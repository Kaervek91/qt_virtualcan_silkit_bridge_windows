cmake_minimum_required(VERSION 3.16)
project(QtSilKitBridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialBus)
find_package(Threads REQUIRED)

# SIL Kit (adjust path to your installation)
set(SILKIT_DIR "C:/Program Files/Vector SIL Kit 5.0.4/x86_64")


add_executable(QtSilKitBridge
    source/main.cpp
    includes/canbridge.h
    # mainwindow.cpp
    # canbridge.cpp
    # silkit_wrapper.cpp
)

target_compile_options(QtSilKitBridge PRIVATE
    /Zi        # debug symbols
    /RTC1      # runtime checks
    /MDd       # debug runtime
)

target_link_options(QtSilKitBridge PRIVATE
    /DEBUG
)

target_include_directories(QtSilKitBridge PRIVATE
    ${SILKIT_DIR}/include
    includes
)

target_link_directories(QtSilKitBridge PRIVATE
    ${SILKIT_DIR}/lib
)
target_link_libraries(QtSilKitBridge
    Qt6::Widgets
    Qt6::SerialBus
    SilKit
    Threads::Threads
)


# =========================
# Qt virtual CAN test
# =========================
add_executable(qt_virtualcan_test
    source/qt_virtualcan_test.cpp
)

target_link_libraries(qt_virtualcan_test PRIVATE
    Qt6::Core
    Qt6::SerialBus
)

# =========================
# SIL Kit test
# =========================
add_executable(silkit_test
    source/silkit_test.cpp
)

target_include_directories(silkit_test PRIVATE
    ${SILKIT_DIR}/include
)

target_link_directories(silkit_test PRIVATE
    ${SILKIT_DIR}/lib
)

target_link_libraries(silkit_test PRIVATE
    SilKit
    Threads::Threads
)
