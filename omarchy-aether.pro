QT += core gui qml quick quickcontrols2 dbus
CONFIG += c++17 release
TARGET = omarchy-aether
TEMPLATE = app
include(src/backend.pri)
SOURCES += src/main.cpp src/cli.cpp
HEADERS += src/cli.h
RESOURCES += src/resources.qrc
