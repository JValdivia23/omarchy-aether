QT += core gui qml quick quickcontrols2 dbus testlib
CONFIG += c++17 testcase
TEMPLATE = app
TARGET = aether_tests
include(../src/backend.pri)
SOURCES += aether_tests.cpp
RESOURCES += ../src/resources.qrc
