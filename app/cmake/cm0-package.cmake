#
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT
# Packaging and install layout for CardputerZero Debian builds.

include(GNUInstallDirs)

set(APP_DISPLAY_NAME "P4wnP1" CACHE STRING "Human-readable application name used by launchers and package filename")
set(APP_DEBIAN_REVISION "rev1" CACHE STRING "Debian package revision/vendor suffix")
set(APP_DEBIAN_ARCHITECTURE "arm64" CACHE STRING "Debian package architecture")
# Deliberately a placeholder: the store rejects packages whose Maintainer is
# still a template/M5Stack identity, so set your own name and email here.
set(APP_MAINTAINER "at0m-b0mb <99875896+at0m-b0mb@users.noreply.github.com>" CACHE STRING "Debian package maintainer")
set(APP_PACKAGE_DESCRIPTION "P4wnP1 A.L.O.A. on-device panel" CACHE STRING "Debian package summary")
set(APP_INSTALL_SYSTEMD_SERVICE ON CACHE BOOL "Install a systemd service file for embedded deployments")

set(APP_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/package")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/templates/app.desktop.in"
    "${APP_GENERATED_DIR}/${PROJECT_NAME}.desktop"
    @ONLY
)

if(APP_INSTALL_SYSTEMD_SERVICE)
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/templates/app.service.in"
        "${APP_GENERATED_DIR}/${PROJECT_NAME}.service"
        @ONLY
    )
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/p4wnp1_panel.conf"
    DESTINATION "/etc"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/audio"
    PATTERN ".DS_Store" EXCLUDE
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/fonts"
    PATTERN ".DS_Store" EXCLUDE
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/images"
    PATTERN ".DS_Store" EXCLUDE
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/APPLaunch/share/images"
    FILES_MATCHING
    PATTERN "p4wnp1_panel*.png"
)

install(FILES "${APP_GENERATED_DIR}/${PROJECT_NAME}.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/APPLaunch/applications"
)

if(APP_INSTALL_SYSTEMD_SERVICE)
    install(FILES "${APP_GENERATED_DIR}/${PROJECT_NAME}.service"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system"
    )
endif()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/LICENSE.txt"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
    RENAME "third-party-assets-license.txt"
)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/dist")
set(CPACK_PACKAGE_NAME "${APP_DISPLAY_NAME}")
set(CPACK_PACKAGE_VENDOR "${APP_MAINTAINER}")
set(CPACK_PACKAGE_CONTACT "${APP_MAINTAINER}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_PACKAGE_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${APP_DISPLAY_NAME}_${PROJECT_VERSION}_${APP_DEBIAN_REVISION}_${APP_DEBIAN_ARCHITECTURE}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/LICENSE.txt")

# The panel ships as its own package so it can be installed alongside the
# P4wnP1 service rather than fighting it for the name.
set(APP_DEBIAN_PACKAGE_NAME "p4wnp1-panel" CACHE STRING "Debian package name")
string(TOLOWER "${APP_DEBIAN_PACKAGE_NAME}" APP_DEBIAN_PACKAGE_NAME)
string(REGEX REPLACE "[^a-z0-9+.-]" "-" APP_DEBIAN_PACKAGE_NAME "${APP_DEBIAN_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_NAME "${APP_DEBIAN_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_RELEASE "${APP_DEBIAN_REVISION}")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${APP_DEBIAN_ARCHITECTURE}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${APP_MAINTAINER}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(APP_DEBIAN_DEPENDS "libc6, libstdc++6, libgcc-s1, libfreetype6, libpng16-16, libjpeg62-turbo, zlib1g")
if(FMT_INCLUDE_DIR AND FMT_LIBRARY)
    string(APPEND APP_DEBIAN_DEPENDS ", libfmt10")
endif()
# Generate Depends from what the binary actually links, and keep the
# hand-written list only as the floor beneath it.
#
# The list above is maintained by hand, which means adding a library to the
# build silently leaves it stale: libcurl arrived with the service client and
# the package went on declaring seven dependencies for a binary that needed
# eight. dpkg would have installed it happily onto a device missing libcurl4
# and the app would have failed at the dynamic linker with nothing explaining
# why. shlibdeps reads the ELF, so it cannot drift.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# When shlibdeps can run it is the only list worth having: it reads the ELF,
# so it cannot go stale the next time a library is linked, and it carries
# minimum versions the hand-written list has no way to know.
#
# The hand-written list is kept only as the fallback for builds where
# dpkg-shlibdeps is not available, and is otherwise left out rather than
# merged, because merging produced duplicate entries.
#
# (An earlier version of this comment claimed the hand-written names did not
# exist on trixie. They do -- libpng16-16 and friends are transitional
# packages. shlibdeps simply resolves to the real ones, libpng16-16t64 and
# libcurl4t64, which is better but was not the bug.)
if(CPACK_DEBIAN_PACKAGE_SHLIBDEPS)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
else()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "${APP_DEBIAN_DEPENDS}")
endif()

# The engine, as a Recommends rather than a Depends.
#
# This package is the FACE: it speaks HTTP RPC to a P4wnP1 service on :8000 and
# contains no part of P4wnP1 itself. Until this line existed, installing it on
# a clean device produced a panel talking to an engine that was not there, with
# nothing in the package metadata to say so.
#
# Recommends, not Depends, because the panel is genuinely useful without the
# engine on the same machine -- pointed at a P4wnP1 on another box it is a
# remote console, and `p4wnp1_panel --restore-networking` must keep working
# even after the engine has been removed. apt installs Recommends by default,
# so `apt install p4wnp1-panel` gets the whole device anyway; someone who
# wants just the face can still pass --no-install-recommends.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "p4wnp1-aloa")

set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/templates/conffiles.in"
    "${APP_GENERATED_DIR}/conffiles"
    @ONLY
)
# Everything that goes in the control archive, set ONCE.
#
# CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA is a plain variable, so a second set()
# silently replaces the first. Adding the maintainer scripts further up and
# leaving this line as it was produced a package with neither of them and no
# warning anywhere -- it built, it installed, and only reading the control
# archive showed they were missing.
#
# prerm asks the panel to hand NetworkManager back while /usr/bin is still
# populated; postrm clears the marker if that did not happen. Without them
# the "restored even when disarm never happens" promise has no removal path,
# which is the gap 0.3.1 had to correct in the README.
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${APP_GENERATED_DIR}/conffiles"
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/prerm"
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postrm")

include(CPack)
