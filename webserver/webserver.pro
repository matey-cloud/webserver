TEMPLATE = app
CONFIG += console c++20
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        blockqueue.cpp \
        httpconnection.cpp \
        listtimer.cpp \
        locker.cpp \
        log.cpp \
        main.cpp \
        threadpool.cpp

HEADERS += \
    blockqueue.h \
    httpconnection.h \
    listtimer.h \
    locker.h \
    log.h \
    threadpool.h
