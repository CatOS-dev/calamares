// SPDX-License-Identifier: GPL-3.0-or-later
#include "PackageChooserHookJob.h"

PackageChooserHookJob::PackageChooserHookJob( const QVariant& script, bool doChroot, int timeout )
    : m_commands( script, doChroot, std::chrono::seconds( timeout ) )
{
}

QString
PackageChooserHookJob::prettyName() const
{
    return tr( "Running selected option…" );
}

Calamares::JobResult
PackageChooserHookJob::exec()
{
    return m_commands.run();
}
