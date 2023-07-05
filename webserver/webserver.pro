TEMPLATE = app
CONFIG += console c++20
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        locker.cpp \
        main.cpp \
        threadpool.cpp

HEADERS += \
    locker.h \
    threadpool.h
