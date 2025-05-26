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
    reg_window.cpp \
    transitwindow.cpp \
    groupchatwindow.cpp \
    chat_controller.cpp \
    mainwindow_controller.cpp \
    controller_manager.cpp \
    privatechat_controller.cpp \
    privatechatwindow.cpp \
    privatechat_model.cpp \

HEADERS += \
    mainwindow.h \
    auth_window.h \
    reg_window.h \
    transitwindow.h \
    groupchatwindow.h \
    chat_controller.h \
    mainwindow_controller.h \
    singleton_helper.h \
    controller_manager.h \
    privatechat_controller.h \
    privatechatwindow.h \
    privatechat_model.h \

FORMS += \
    auth_window.ui \
    groupchatwindow.ui \
    privatechatwindow.ui \
    reg_window.ui \
    transitwindow.ui \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
