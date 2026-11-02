// SPDX-License-Identifier: GPL-3.0-or-later
#include "SummaryViewStep.h"
#include "PackageCheck.h"

#include "Branding.h"
#include "GlobalStorage.h"
#include "JobQueue.h"
#include "Settings.h"
#include "ViewManager.h"

#include <QLabel>
#include <QtTest/QtTest>

class SummaryTests : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testChecksAndRevisits();
};

void SummaryTests::initTestCase()
{
    Calamares::Settings::init( QStringLiteral( SOURCE_DIRECTORY "settings.conf" ) );
    new Calamares::Branding( QStringLiteral( SOURCE_DIRECTORY "src/branding/default/branding.desc" ), this );
    new Calamares::JobQueue( this );
    Calamares::ViewManager::instance( this );
}

void SummaryTests::testChecksAndRevisits()
{
    QLocale::setDefault( QLocale( "zh_CN" ) );
    SummaryViewStep step;
    const auto originalTitle = step.prettyName();
    const auto originalDescription = step.widget()->findChild< QLabel* >( "summaryTitle" )->text();
    step.setConfigurationMap( QVariantMap { { "packageCheck", QVariantMap {
        { "rules", QVariantList {
            QVariantMap { { "when", "package('alpha') && package('beta')" }, { "level", "error" },
                          { "message", "Conflict" }, { "message[zh_CN]", "不能同时选择这两项。" } },
            QVariantMap { { "when", "package('gamma') && !package('removed')" }, { "level", "warning" },
                          { "message", "Warning message" }, { "message[zh_CN]", "安装后需要设置默认程序。" } }
        } }
    } } } );
    auto* gs = Calamares::JobQueue::instance()->globalStorage();
    gs->insert( "packageOperations", QVariantList {
        QVariantMap { { "source", "netinstall@test" }, { "install", QVariantList { "alpha" } } },
        QVariantMap { { "try_install", QVariantList { QVariantMap { { "package", "beta" } }, "gamma" } } },
        QVariantMap { { "remove", QVariantList { "removed" } } }
    } );
    step.onActivate();
    QVERIFY( !step.isNextEnabled() );
    QCOMPARE( step.packageNotices().size(), 2 );
    QCOMPARE( step.packageNotices().at( 0 ).message, QStringLiteral( "不能同时选择这两项。" ) );
    QCOMPARE( step.prettyName(), originalTitle );
    auto* description = step.widget()->findChild< QLabel* >( "summaryTitle" );
    QCOMPARE( description->text(), originalDescription );
    QVERIFY( !description->isHidden() );
    QCOMPARE( step.widget()->findChildren< QLabel* >( "packageCheckRow" ).size(), 2 );
    step.onLeave();

    gs->insert( "packageOperations", QVariantList { QVariantMap { { "install", QVariantList { "gamma" } } } } );
    step.onActivate();
    QVERIFY( step.isNextEnabled() );
    QCOMPARE( step.packageNotices().size(), 1 );
    QCOMPARE( step.widget()->findChildren< QLabel* >( "packageCheckRow" ).size(), 1 );
    step.onLeave();

    gs->insert( "packageOperations", QVariantList() );
    step.onActivate();
    QVERIFY( step.isNextEnabled() );
    QVERIFY( step.packageNotices().isEmpty() );
    QCOMPARE( step.widget()->findChildren< QLabel* >( "packageCheckRow" ).size(), 1 );
    step.onLeave();
}

QTEST_MAIN( SummaryTests )
#include "Tests.moc"
