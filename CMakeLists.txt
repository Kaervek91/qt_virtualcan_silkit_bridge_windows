cmake_minimum_required(VERSION 3.16)
project(QtSilKitBridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

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
    $<$<CONFIG:Debug>:/Zi /RTC1 /MDd>
    $<$<CONFIG:Release>:/O2 /MD>
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


if(WIN32)

    # Path to windeployqt
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "C:/Qt/6.11.0/msvc2022_64/bin")

    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt not found")
    endif()

    # Path to SIL Kit DLLs
    set(SILKIT_BIN_DIR "C:/Program Files/Vector SIL Kit 5.0.4/x86_64/bin")

    # Deploy folder path
    set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/package/QtSilKitBridge_release")

    # Copy exe release
    add_custom_command(TARGET QtSilKitBridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}"
    
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:QtSilKitBridge>
            "${PACKAGE_DIR}"
    
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
            #--release
            #--no-translations
            #--compiler-runtime
            "${PACKAGE_DIR}/QtSilKitBridge.exe"
    
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SILKIT_BIN_DIR}/SilKit.dll"
            "${PACKAGE_DIR}"
    )

endif()
