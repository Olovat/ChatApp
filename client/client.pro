QT       += core gui network sql testlib

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    auth_window/qt_auth_window.cpp \
    groupchat_window/qt_group_chat_window.cpp \
    main.cpp \
    mainwindow/qt_main_window.cpp \
    privatechat_window/qt_private_chat_window.cpp \
    reg_window/qt_reg_window.cpp \
    transit_window/transitwindow.cpp \

HEADERS += \
    auth_window/auth_window_interface.h \
    auth_window/qt_auth_window.h \
    groupchat_window/group_chat_interface.h \
    groupchat_window/qt_group_chat_window.h \
    mainwindow/main_window_interface.h \
    mainwindow/qt_main_window.h \
    privatechat_window/private_chat_interface.h \
    privatechat_window/qt_private_chat_window.h \
    reg_window/qt_reg_window.h \
    reg_window/reg_window_interface.h \
    transit_window/transitwindow.h \

FORMS += \
    groupchat_window/groupchatwindow.ui \
    mainwindow/mainwindow.ui \
    auth_window/auth_window.ui \
    privatechat_window/privatechatwindow.ui \
    reg_window/reg_window.ui \
    transit_window/transitwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
