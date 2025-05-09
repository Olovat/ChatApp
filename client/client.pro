QT       += core gui network sql testlib

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    auth_window.cpp \
    privatechatwindow.cpp \
    reg_window.cpp \
    transitwindow.cpp \
    groupchatwindow.cpp

HEADERS += \
    mainwindow.h \
    auth_window.h \
    privatechatwindow.h \
    reg_window.h \
    transitwindow.h \
    groupchatwindow.h

FORMS += \
    groupchatwindow.ui \
    mainwindow.ui \
    auth_window.ui \
    privatechatwindow.ui \
    reg_window.ui \
    transitwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
