#/****************************************************************************
#
#    flow5 application
#    Copyright © 2025 André Deperrois
#    All rights reserved.
#
#*****************************************************************************/




HEADERS += \
    $$PWD/afmesher.h \
    $$PWD/mesherwt.h \
    $$PWD/meshevent.h \
    $$PWD/occtessctrlswt.h \
    $$PWD/panelcheckdlg.h \
    $$PWD/slg3d.h \
    $$PWD/tesscontrolsdlg.h



SOURCES += \
    $$PWD/afmesher.cpp \
    $$PWD/mesherwt.cpp \
    $$PWD/occtessctrlswt.cpp \
    $$PWD/panelcheckdlg.cpp \
    $$PWD/slg3d.cpp \
    $$PWD/tesscontrolsdlg.cpp


# GMSH-dependent files - excluded when NO_GMSH is defined
!NO_GMSH {
    HEADERS += \
        $$PWD/gmesh_globals.h \
        $$PWD/gmesher.h \
        $$PWD/gmesherwt.h \
        $$PWD/gmshctrlswt.h

    SOURCES += \
        $$PWD/gmesh_globals.cpp \
        $$PWD/gmesher.cpp \
        $$PWD/gmesherwt.cpp \
        $$PWD/gmshctrlswt.cpp
}
