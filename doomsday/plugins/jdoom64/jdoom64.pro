# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../pluginconfig.pri)
include(../common/common.pri)

TEMPLATE = lib
TARGET = jdoom64

DEFINES += __JDOOM64__

VERSION = $$JDOOM64_VERSION

gamedata.files = $$OUT_PWD/../../jdoom64.pk3

macx {
    gamedata.path = Contents/Resources

    QMAKE_BUNDLE_DATA += gamedata
}
#unix:!macx {
#    target.path = $$DENG_LIB_DIR
#    gamedata.path = $$DENG_DATA_DIR/jdoom64
#
#    INSTALLS += target gamedata
#}

INCLUDEPATH += include

HEADERS += \
    include/acfnlink.h \
    include/d_api.h \
    include/d_config.h \
    include/d_console.h \
    include/d_englsh.h \
    include/d_items.h \
    include/d_main.h \
    include/d_player.h \
    include/d_refresh.h \
    include/d_think.h \
    include/doomdef.h \
    include/doomtype.h \
    include/dstrings.h \
    include/g_ctrl.h \
    include/g_game.h \
    include/info.h \
    include/jdoom64.h \
    include/m_cheat.h \
    include/m_menu.h \
    include/m_random.h \
    include/mn_def.h \
    include/p_enemy.h \
    include/p_inter.h \
    include/p_lights.h \
    include/p_maputl.h \
    include/p_mobj.h \
    include/p_pspr.h \
    include/p_setup.h \
    include/p_sound.h \
    include/p_spec.h \
    include/p_telept.h \
    include/r_defs.h \
    include/st_stuff.h \
    include/tables.h \
    include/version.h \
    include/wi_stuff.h

SOURCES += \
    src/acfnlink.c \
    src/d_api.c \
    src/d_console.c \
    src/d_items.c \
    src/d_main.c \
    src/d_refresh.c \
    src/g_ctrl.c \
    src/m_cheat.c \
    src/m_random.c \
    src/p_enemy.c \
    src/p_inter.c \
    src/p_lights.c \
    src/p_maputl.c \
    src/p_mobj.c \
    src/p_pspr.c \
    src/p_setup.c \
    src/p_sound.c \
    src/p_spec.c \
    src/p_telept.c \
    src/st_stuff.c \
    src/tables.c \
    src/wi_stuff.c \
