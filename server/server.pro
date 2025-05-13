QT = core
QT += core network sql

CONFIG += c++17 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ChatLogicServer.cpp \
    main.cpp \
    qt_database_adapter.cpp \
    qt_network_adapter.cpp \


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    qt_network_adapter.h \
    qt_database_adapter.h \
    network_interface.h \
    database_interface.h \
    chat_logic_server.h \

