QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    codeeditor.cpp \
    infowindow.cpp \
    lexer.cpp \
    main.cpp \
    mainwindow.cpp \
    parser.cpp

HEADERS += \
    codeeditor.h \
    infowindow.h \
    lexer.h \
    mainwindow.h \
    parser.h \
    token.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    Compiler_ru_RU.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    README.md \
    TEXT_TASK.md \
    TEXT_GRAMMAR.md \
    TEXT_GRAMMAR_fsm.png \
    TEXT_CLASSIFICATION.md \
    TEXT_METHOD.md \
    TEXT_METHOD_fsm.png \
    TEXT_TEST.md \
    TEXT_REFERENCES.md \
    TEXT_SOURCE.md

RESOURCES += \
    README.qrc
