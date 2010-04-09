INCLUDEPATH += $$PWD/../../../libs/3rdparty/botan/build
INCLUDEPATH += $$PWD/../../../libs/3rdparty/net7ssh/src
LIBS += -l$$qtLibraryTarget(Net7ssh) -l$$qtLibraryTarget(Botan)

HEADERS += \
    $$PWD/maemoconfigtestdialog.h \
    $$PWD/maemoconstants.h \
    $$PWD/maemodeviceconfigurations.h \
    $$PWD/maemomanager.h \
    $$PWD/maemorunconfiguration.h \
    $$PWD/maemorunconfigurationwidget.h \
    $$PWD/maemoruncontrol.h \
    $$PWD/maemorunfactories.h \
    $$PWD/maemosettingspage.h \
    $$PWD/maemosettingswidget.h \
    $$PWD/maemosshconnection.h \
    $$PWD/maemosshthread.h \
    $$PWD/maemotoolchain.h \
    $$PWD/maemopackagecreationstep.h \
    $$PWD/maemopackagecreationfactory.h \
    $$PWD/ne7sshobject.h

SOURCES += \
    $$PWD/maemoconfigtestdialog.cpp \
    $$PWD/maemodeviceconfigurations.cpp \
    $$PWD/maemomanager.cpp \
    $$PWD/maemorunconfiguration.cpp \
    $$PWD/maemorunconfigurationwidget.cpp \
    $$PWD/maemoruncontrol.cpp \
    $$PWD/maemorunfactories.cpp \
    $$PWD/maemosettingspage.cpp \
    $$PWD/maemosettingswidget.cpp \
    $$PWD/maemosshconnection.cpp \
    $$PWD/maemosshthread.cpp \
    $$PWD/maemotoolchain.cpp \
    $$PWD/maemopackagecreationstep.cpp \
    $$PWD/maemopackagecreationfactory.cpp \
    $$PWD/ne7sshobject.cpp

FORMS += \
    $$PWD/maemoconfigtestdialog.ui \
    $$PWD/maemosettingswidget.ui

RESOURCES += $$PWD/qt-maemo.qrc
