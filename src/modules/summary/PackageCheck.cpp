// SPDX-License-Identifier: GPL-3.0-or-later
#include "PackageCheck.h"

#include "locale/TranslatableConfiguration.h"
#include "utils/Logger.h"
#include "compat/Variant.h"

#include <QCoreApplication>
#include <QJSEngine>
#include <QSet>

QList< PackageCheckNotice >
checkPackages( const QVariantList& operations, const QVariantList& rules )
{
    QSet< QString > packages;
    for ( const auto& operation : operations )
    {
        const auto map = operation.toMap();
        for ( const auto* action : { "install", "try_install" } )
        {
            for ( const auto& item : map.value( action ).toList() )
            {
                const auto name = Calamares::typeOf( item ) == Calamares::StringVariantType
                    ? item.toString() : item.toMap().value( "package" ).toString();
                packages.insert( name );
            }
        }
    }

    QJSEngine engine;
    auto factory = engine.evaluate( QStringLiteral(
        "(function(names) { return function(name) { return names.includes(name); }; })" ) );
    engine.globalObject().setProperty( "package", factory.call( { engine.toScriptValue( packages.values() ) } ) );

    QList< PackageCheckNotice > notices;
    for ( const auto& entry : rules )
    {
        const auto rule = entry.toMap();
        const auto level = rule.value( "level" ).toString();
        const auto expression = rule.value( "when" ).toString();
        const auto message = Calamares::Locale::TranslatedString( rule, "message" ).get();
        const auto result = engine.evaluate( expression );
        if ( !result.isBool() || ( level != "error" && level != "warning" ) || message.isEmpty() )
        {
            cWarning() << "Invalid package check rule:" << expression << result.toString();
            notices.append( { true, QCoreApplication::translate( "PackageCheck",
                "Package checks could not be completed. Please contact the distribution maintainer." ) } );
            break;
        }
        if ( result.toBool() )
        {
            notices.append( { level == "error", message } );
        }
    }
    return notices;
}
