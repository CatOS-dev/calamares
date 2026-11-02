# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
find_package(${qtname} COMPONENTS Qml REQUIRED)

include_directories(${PROJECT_BINARY_DIR}/src/libcalamaresui)
calamares_add_plugin(summary
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        Config.cpp
        PackageCheck.cpp
        SummaryModel.cpp
        SummaryPage.cpp
        SummaryViewStep.cpp
    UI
    SHARED_LIB
    LINK_PRIVATE_LIBRARIES ${qtname}::Qml
)

calamares_add_test(
    summarytest
    GUI
    SOURCES Tests.cpp
    LIBRARIES ${summary_TARGET}
    DEFINITIONS SOURCE_DIRECTORY="${CMAKE_SOURCE_DIR}/"
)
