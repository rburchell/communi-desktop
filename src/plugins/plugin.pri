######################################################################
# Communi
######################################################################

TEMPLATE = lib
CONFIG += plugin static
TARGET = $$basename(_PRO_FILE_PWD_)plugin

DEPENDPATH += $$_PRO_FILE_PWD_
INCLUDEPATH += $$_PRO_FILE_PWD_

DESTDIR = $$PWD/../../plugins

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x000000
}

include(../config.pri)
include(../libs/api/api.pri)
include(../libs/backend/backend.pri)