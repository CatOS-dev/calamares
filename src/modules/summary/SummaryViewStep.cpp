/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "SummaryViewStep.h"

#include "SummaryPage.h"
#include "GlobalStorage.h"
#include "JobQueue.h"

#include <algorithm>

CALAMARES_PLUGIN_FACTORY_DEFINITION( SummaryViewStepFactory, registerPlugin< SummaryViewStep >(); )

SummaryViewStep::SummaryViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
    , m_config( new Config( this ) )
    , m_widget( new SummaryPage( m_config ) )
{
    emit nextStatusChanged( true );
}


SummaryViewStep::~SummaryViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
    delete m_config;
}


QString
SummaryViewStep::prettyName() const
{
    return m_config->title();
}


QWidget*
SummaryViewStep::widget()
{
    return m_widget;
}


bool
SummaryViewStep::isNextEnabled() const
{
    return !m_packageErrors;
}


bool
SummaryViewStep::isBackEnabled() const
{
    return true;
}


bool
SummaryViewStep::isAtBeginning() const
{
    return true;
}


bool
SummaryViewStep::isAtEnd() const
{
    return true;
}


Calamares::JobList
SummaryViewStep::jobs() const
{
    return {};
}


void
SummaryViewStep::onActivate()
{
    m_packageNotices.clear();
    if ( m_packageCheckEnabled )
    {
        auto* gs = Calamares::JobQueue::instance()->globalStorage();
        m_packageNotices = checkPackages( gs->value( "packageOperations" ).toList(), m_packageRules );
    }
    m_packageErrors = std::any_of( m_packageNotices.cbegin(), m_packageNotices.cend(),
                                  []( const auto& notice ) { return notice.error; } );
    emit nextStatusChanged( isNextEnabled() );
    m_config->collectSummaries( this, Config::Widgets::Enabled );
    m_widget->buildWidgets( m_config, this );
}


void
SummaryViewStep::onLeave()
{
    m_config->clearSummaries();
    m_widget->cleanup();
}

void
SummaryViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_packageCheckEnabled = configurationMap.contains( "packageCheck" );
    m_packageRules = configurationMap.value( "packageCheck" ).toMap().value( "rules" ).toList();
}
