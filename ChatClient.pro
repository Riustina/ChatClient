QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    global.cpp \
    httpmgr.cpp \
    logindialog.cpp \
    main.cpp \
    mainwindow.cpp \
    registerdialog.cpp \
    resetdialog.cpp

HEADERS += \
    Singleton.h \
    global.h \
    httpmgr.h \
    logindialog.h \
    mainwindow.h \
    registerdialog.h \
    resetdialog.h

FORMS += \
    logindialog.ui \
    mainwindow.ui \
    registerdialog.ui \
    resetdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    rc.qrc

DISTFILES += \
    config.ini


# 仅针对 Windows 平台处理
win32 {
    # 1. 定义源文件路径 (工程目录下的 config.ini)
    CONFIG_SOURCE = $$PWD/config.ini
    CONFIG_SOURCE = $$replace(CONFIG_SOURCE, /, \\)

    # 2. 定义目标输出路径
    # 注意：$$OUT_PWD 是构建目录，通常可执行文件就在这里或其子目录下
    CONFIG_DEST = $$OUT_PWD

    # 如果你手动设置了 DESTDIR，或者在不同模式下生成到了 debug/release 文件夹
    # 这一步能确保无论在哪个模式，都能找到可执行文件所在的真实位置
    CONFIG(debug, debug|release) {
        CONFIG_DEST = $$OUT_PWD/debug
    } else {
        CONFIG_DEST = $$OUT_PWD/release
    }

    CONFIG_DEST = $$replace(CONFIG_DEST, /, \\)

    # 3. 编写拷贝命令
    # /Y 表示覆盖不提示
    QMAKE_POST_LINK += copy /Y \"$$CONFIG_SOURCE\" \"$$CONFIG_DEST\"
}
