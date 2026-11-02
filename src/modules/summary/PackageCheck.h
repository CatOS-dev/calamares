// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SUMMARY_PACKAGECHECK_H
#define SUMMARY_PACKAGECHECK_H

#include <QList>
#include <QString>
#include <QVariant>

struct PackageCheckNotice
{
    bool error;
    QString message;
};

QList< PackageCheckNotice > checkPackages( const QVariantList& operations, const QVariantList& rules );

#endif
