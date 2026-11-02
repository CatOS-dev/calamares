// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PACKAGECHOOSER_HOOKJOB_H
#define PACKAGECHOOSER_HOOKJOB_H

#include "Job.h"
#include "utils/CommandList.h"

class PackageChooserHookJob : public Calamares::Job
{
public:
    PackageChooserHookJob( const QVariant& script, bool doChroot, int timeout );
    QString prettyName() const override;
    Calamares::JobResult exec() override;

private:
    Calamares::CommandList m_commands;
};

#endif
