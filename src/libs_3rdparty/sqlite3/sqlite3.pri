# include (sqlite.pri)
include( ../../ugene_globals.pri )
UGENE_RELATIVE_DESTDIR = ''

TARGET = ugene_db
TEMPLATE = lib
CONFIG +=thread debug_and_release
INCLUDEPATH += src
DEFINES+=SQLITE_ENABLE_COLUMN_METADATA
LIBS += -L../../_release

!debug_and_release|build_pass {

    CONFIG(debug, debug|release) {
        TARGET = ugene_dbd
        DEFINES+=_DEBUG
        CONFIG +=console
        DESTDIR=../../_debug/
        OBJECTS_DIR=_tmp/obj/debug
        LIBS -= -L../../_release 
        LIBS += -L../../_debug
    }

    CONFIG(release, debug|release) {
        TARGET = ugene_db
        DEFINES+=NDEBUG
        DESTDIR=../../_release/
        OBJECTS_DIR=_tmp/obj/release
    }
}


win32 {
    DEF_FILE=src/sqlite3.def

    QMAKE_CXXFLAGS_WARN_ON = -W3
    QMAKE_CFLAGS_WARN_ON = -W3

    QMAKE_MSVC_PROJECT_NAME=lib_3rd_sqlite3
}


unix {
    target.path = $$UGENE_INSTALL_DIR/$$UGENE_RELATIVE_DESTDIR
    INSTALLS += target
}
