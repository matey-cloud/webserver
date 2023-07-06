TEMPLATE = app
CONFIG += console c++20
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        httpconnection.cpp \
        locker.cpp \
        main.cpp \
        threadpool.cpp

HEADERS += \
    httpconnection.h \
    locker.h \
    threadpool.h
