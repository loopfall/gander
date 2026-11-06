QT += core gui qml quick quickcontrols2 dbus multimedia concurrent svg

# qtquickcompiler turns the QML into C++ at build time, so the daemon never
# parses QML at startup. Poppler renders PDFs (and Office documents once
# LibreOffice has turned them into PDF), TagLib reads tags and cover art.
CONFIG += c++17 release qtquickcompiler link_pkgconfig
PKGCONFIG += poppler-qt6 taglib
LIBS += -lLayerShellQtInterface

TARGET = gander
TEMPLATE = app
VERSION = 0.1.0
DEFINES += GANDER_VERSION=\\\"$$VERSION\\\"

HEADERS += \
    src/audio.h \
    src/dbusadaptor.h \
    src/imagestore.h \
    src/inspector.h \
    src/listing.h \
    src/office.h \
    src/overlay.h \
    src/pdf.h \
    src/previewer.h \
    src/tablemodel.h \
    src/textloader.h \
    src/highlighter.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/audio.cpp \
    src/dbusadaptor.cpp \
    src/imagestore.cpp \
    src/inspector.cpp \
    src/listing.cpp \
    src/office.cpp \
    src/overlay.cpp \
    src/pdf.cpp \
    src/previewer.cpp \
    src/tablemodel.cpp \
    src/textloader.cpp \
    src/highlighter.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
