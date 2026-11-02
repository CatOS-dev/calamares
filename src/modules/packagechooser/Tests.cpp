/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "Tests.h"

#ifdef HAVE_APPDATA
#include "ItemAppData.h"
#endif
#ifdef HAVE_APPSTREAM_VERSION
#include "ItemAppStream.h"
#endif
#include "Config.h"
#include "PackageModel.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"

#include <QtTest/QtTest>

QTEST_GUILESS_MAIN( PackageChooserTests )

PackageChooserTests::PackageChooserTests() {}

PackageChooserTests::~PackageChooserTests() {}

void
PackageChooserTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );
    if ( !Calamares::JobQueue::instance() )
    {
        (void)new Calamares::JobQueue();
    }
}

void
PackageChooserTests::testBogus()
{
    QVERIFY( true );
}

void
PackageChooserTests::testAppData()
{
    // Path from the build-dir and from the running-the-test varies,
    // for in-source build, for build/, and for tests-in-build/,
    // so look in multiple places.
    //
    // CMake adds SOURCE_DIRECTORY as a compile-time definition.
    const QString appdataBaseName( "io.calamares.calamares.appdata.xml" );
    QString appdataName;
    for ( const auto& prefix : QStringList { SOURCE_DIRECTORY, "", "../", "../../../", "../../../../" } )
    {
        const auto candidate = prefix + appdataBaseName;
        if ( QFile::exists( candidate ) )
        {
            appdataName = candidate;
            break;
        }
    }
    QVERIFY( !appdataName.isEmpty() );
    QVERIFY( QFile::exists( appdataName ) );

    QVariantMap m;
    m.insert( "appdata", appdataName );

#ifdef HAVE_XML
    PackageItem p1 = fromAppData( m );
    QVERIFY( p1.isValid() );
    QCOMPARE( p1.id, QStringLiteral( "io.calamares.calamares.desktop" ) );
    QCOMPARE( p1.name.get(), QStringLiteral( "Calamares" ) );
    // The <description> entry has precedence
    QCOMPARE( p1.description.get(), QStringLiteral( "Calamares is an installer program for Linux distributions." ) );
    // .. but en_GB doesn't have an entry in description, so uses <summary>
    QCOMPARE( p1.description.get( QLocale( "en_GB" ) ), QStringLiteral( "Calamares Linux Installer" ) );
    QCOMPARE( p1.description.get( QLocale( "nl" ) ),
              QStringLiteral( "Calamares is een installatieprogramma voor Linux distributies." ) );
    QVERIFY( p1.screenshot.isNull() );

    m.insert( "id", "calamares" );
    m.insert( "screenshot", ":/images/calamares.png" );
    PackageItem p2 = fromAppData( m );
    QVERIFY( p2.isValid() );
    QCOMPARE( p2.id, QStringLiteral( "calamares" ) );
    QCOMPARE( p2.description.get( QLocale( "nl" ) ),
              QStringLiteral( "Calamares is een installatieprogramma voor Linux distributies." ) );
    QVERIFY( !p2.screenshot.isNull() );
#endif
}

void
PackageChooserTests::testVisibilityEqualityExpression()
{
    auto* gs = Calamares::JobQueue::instance()->globalStorage();
    gs->insert( QStringLiteral( "firmwareType" ), QStringLiteral( "bios" ) );
    gs->insert( QStringLiteral( "secureboot.enabled" ), false );

    QVariantList items {
        QVariantMap {
            { QStringLiteral( "id" ), QStringLiteral( "grub" ) },
            { QStringLiteral( "name" ), QStringLiteral( "GRUB" ) },
        },
        QVariantMap {
            { QStringLiteral( "id" ), QStringLiteral( "uki" ) },
            { QStringLiteral( "name" ), QStringLiteral( "Unified Kernel Image" ) },
            { QStringLiteral( "visibleWhen" ), QStringList { QStringLiteral( "firmwareType == 'efi'" ) } },
        },
    };
    Config config;
    config.setConfigurationMap( QVariantMap { { QStringLiteral( "items" ), items } } );

    QCOMPARE( config.model()->packageCount(), 1 );
    QCOMPARE( config.model()->packageData( 0 ).id, QStringLiteral( "grub" ) );
}

void
PackageChooserTests::testVisibilityInequalityExpression()
{
    auto* gs = Calamares::JobQueue::instance()->globalStorage();
    gs->insert( QStringLiteral( "firmwareType" ), QStringLiteral( "efi" ) );
    gs->insert( QStringLiteral( "secureboot.enabled" ), true );

    QVariantList items {
        QVariantMap {
            { QStringLiteral( "id" ), QStringLiteral( "grub" ) },
            { QStringLiteral( "name" ), QStringLiteral( "GRUB" ) },
        },
        QVariantMap {
            { QStringLiteral( "id" ), QStringLiteral( "uki" ) },
            { QStringLiteral( "name" ), QStringLiteral( "Unified Kernel Image" ) },
            { QStringLiteral( "visibleWhen" ), QStringList { QStringLiteral( "firmwareType == 'efi'" ) } },
        },
        QVariantMap {
            { QStringLiteral( "id" ), QStringLiteral( "efistub" ) },
            { QStringLiteral( "name" ), QStringLiteral( "Linux EFI Stub" ) },
            { QStringLiteral( "visibleWhen" ),
              QStringList { QStringLiteral( "firmwareType == 'efi'" ),
                            QStringLiteral( "secureboot.enabled != true" ) } },
        },
    };
    Config config;
    config.setConfigurationMap( QVariantMap { { QStringLiteral( "items" ), items } } );

    QCOMPARE( config.model()->packageCount(), 2 );
    QCOMPARE( config.model()->packageData( 0 ).id, QStringLiteral( "grub" ) );
    QCOMPARE( config.model()->packageData( 1 ).id, QStringLiteral( "uki" ) );
}

void
PackageChooserTests::testNetinstallSelectionIsAvailableToContextualProcess()
{
    auto* gs = Calamares::JobQueue::instance()->globalStorage();
    gs->remove( QStringLiteral( "netinstallSelect" ) );
    gs->remove( QStringLiteral( "packagechooser_desktop" ) );

    Config config;
    config.setDefaultId( Calamares::ModuleSystem::InstanceKey( QStringLiteral( "packagechooser" ),
                                                               QStringLiteral( "desktop" ) ) );
    config.setConfigurationMap(
        QVariantMap { { QStringLiteral( "method" ), QStringLiteral( "netinstall-select" ) } } );
    config.updateGlobalStorage( QStringList { QStringLiteral( "Niri-dms" ) } );

    QCOMPARE( gs->value( QStringLiteral( "netinstallSelect" ) ).toStringList(),
              QStringList { QStringLiteral( "Niri-dms" ) } );
    QCOMPARE( gs->value( QStringLiteral( "packagechooser_desktop" ) ).toString(),
              QStringLiteral( "Niri-dms" ) );
}

void
PackageChooserTests::testNetinstallSelectionReplacesOwnPreviousSelection()
{
    auto* gs = Calamares::JobQueue::instance()->globalStorage();
    gs->remove( QStringLiteral( "netinstallSelect" ) );
    gs->remove( QStringLiteral( "packagechooser_desktop" ) );
    gs->remove( QStringLiteral( "packagechooser_other" ) );

    Config otherConfig;
    otherConfig.setDefaultId( Calamares::ModuleSystem::InstanceKey( QStringLiteral( "packagechooser" ),
                                                                    QStringLiteral( "other" ) ) );
    otherConfig.setConfigurationMap(
        QVariantMap { { QStringLiteral( "method" ), QStringLiteral( "netinstall-select" ) } } );
    otherConfig.updateGlobalStorage( QStringList { QStringLiteral( "Other-Selection" ) } );

    Config desktopConfig;
    desktopConfig.setDefaultId( Calamares::ModuleSystem::InstanceKey( QStringLiteral( "packagechooser" ),
                                                                      QStringLiteral( "desktop" ) ) );
    desktopConfig.setConfigurationMap(
        QVariantMap { { QStringLiteral( "method" ), QStringLiteral( "netinstall-select" ) } } );
    desktopConfig.updateGlobalStorage( QStringList { QStringLiteral( "KDE-Desktop" ) } );

    desktopConfig.updateGlobalStorage( QStringList { QStringLiteral( "Niri-noctalia" ) } );

    const QStringList expected {
        QStringLiteral( "Other-Selection" ),
        QStringLiteral( "Niri-noctalia" ),
    };
    QCOMPARE( gs->value( QStringLiteral( "netinstallSelect" ) ).toStringList(), expected );
    QCOMPARE( gs->value( QStringLiteral( "packagechooser_desktop" ) ).toString(),
              QStringLiteral( "Niri-noctalia" ) );
}

void
PackageChooserTests::testFinalSelectionHooks()
{
    QTemporaryDir directory;
    QVERIFY( directory.isValid() );
    const QString output = directory.filePath( "result" );
    const QString script = directory.filePath( "action.sh" );
    QFile file( script );
    QVERIFY( file.open( QIODevice::WriteOnly ) );
    file.write( "printf '%s' executed > \"$1\"\n" );
    file.close();

    Config config;
    config.setDefaultId( Calamares::ModuleSystem::InstanceKey( "packagechooser", "hook-test" ) );
    config.setConfigurationMap( QVariantMap {
        { "method", "netinstall-select" },
        { "dontChroot", true },
        { "items", QVariantList {
            QVariantMap { { "id", "old" }, { "name", "Old option" }, { "hook", "false" } },
            QVariantMap { { "id", "final" }, { "name", "Final option" },
                          { "hook", QString( "bash '%1' '%2'" ).arg( script, output ) } },
            QVariantMap { { "id", "plain" }, { "name", "Plain option" } }
        } }
    } );
    config.updateGlobalStorage( { "old" } );
    config.updateGlobalStorage( { "final" } );
    QVERIFY( !QFile::exists( output ) );
    const auto jobs = config.hookJobs();
    QCOMPARE( jobs.count(), 1 );
    QVERIFY( jobs.first()->exec().succeeded() );
    QFile result( output );
    QVERIFY( result.open( QIODevice::ReadOnly ) );
    QCOMPARE( result.readAll(), QByteArray( "executed" ) );
    config.updateGlobalStorage( { "plain" } );
    QVERIFY( config.hookJobs().isEmpty() );
    config.updateGlobalStorage( { "old" } );
    const auto failingJobs = config.hookJobs();
    QCOMPARE( failingJobs.count(), 1 );
    QVERIFY( !failingJobs.first()->exec().succeeded() );
}
