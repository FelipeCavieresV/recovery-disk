cmake_minimum_required(VERSION 3.20)

project(RecoveryDisk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

qt_standard_project_setup()

qt_add_executable(RecoveryDisk
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/models/RecoverableFile.h

    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/ScanWorker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/ScanWorker.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/MainWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/MainWindow.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/DriveScanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/DriveScanner.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/FileTypeHelper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/FileTypeHelper.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/FileGroupingHelper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/FileGroupingHelper.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/RecycleBinScanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/RecycleBinScanner.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/FolderScanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/FolderScanner.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/DeepDiskScanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/DeepDiskScanner.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/FileRecoveryService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/FileRecoveryService.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/signatures/FileSignature.h

    ${CMAKE_CURRENT_SOURCE_DIR}/src/signatures/SignatureDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/signatures/SignatureDatabase.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/signatures/SignatureScanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/signatures/SignatureScanner.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/preview/PreviewService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/preview/PreviewService.cpp
)

target_include_directories(RecoveryDisk
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RecoveryDisk
    PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)