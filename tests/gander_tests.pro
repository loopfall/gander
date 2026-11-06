QT += core gui quick quickcontrols2 testlib dbus concurrent
CONFIG += c++17 testcase link_pkgconfig
PKGCONFIG += taglib
TARGET = gander_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/inspector.h ../src/textloader.h ../src/listing.h ../src/audio.h ../src/imagestore.h \
           ../src/office.h ../src/tablemodel.h ../src/theme.h ../src/translator.h
SOURCES += gander_tests.cpp ../src/inspector.cpp ../src/textloader.cpp ../src/listing.cpp ../src/audio.cpp \
           ../src/imagestore.cpp ../src/office.cpp ../src/tablemodel.cpp ../src/theme.cpp ../src/translator.cpp
