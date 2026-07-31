/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 *
 */

#include "Runner.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <QElapsedTimer>
#include <QProcess>

#include <algorithm>

/** @brief Descend from directory, always relative
 *
 * If @p subdir begins with a "/" or "../" or "./" those are stripped
 * until none are left, then changes @p directory into that
 * subdirectory.
 *
 * Returns @c false if the @p subdir doesn't make sense.
 */
STATICTEST bool
relativeChangeDirectory( QDir& directory, const QString& subdir )
{
    const QString rootPath = directory.absolutePath();
    const QString concatenatedPath = rootPath + '/' + subdir;
    const QString relPath = QDir::cleanPath( concatenatedPath );

    if ( !relPath.startsWith( rootPath ) )
    {
        cWarning() << "Relative path" << subdir << "escapes from" << rootPath;
        return false;
    }

    return directory.cd( relPath );
}

STATICTEST std::pair< bool, QDir >
calculateWorkingDirectory( Calamares::Utils::RunLocation location, const QString& directory )
{
    Calamares::GlobalStorage* gs
        = Calamares::JobQueue::instance() ? Calamares::JobQueue::instance()->globalStorage() : nullptr;

    if ( location == Calamares::Utils::RunLocation::RunInTarget )
    {
        if ( !gs || !gs->contains( "rootMountPoint" ) )
        {
            cWarning() << "No rootMountPoint in global storage, while RunInTarget is specified";
            return std::make_pair( false, QDir() );
        }

        QDir rootMountPoint( gs->value( "rootMountPoint" ).toString() );
        if ( !rootMountPoint.exists() )
        {
            cWarning() << "rootMountPoint points to a dir which does not exist";
            return std::make_pair( false, QDir() );
        }

        if ( !directory.isEmpty() )
        {

            if ( !relativeChangeDirectory( rootMountPoint, directory ) || !rootMountPoint.exists() )
            {
                cWarning() << "Working directory" << directory << "does not exist in target";
                return std::make_pair( false, QDir() );
            }
        }
        return std::make_pair( true, rootMountPoint );  // Now changed to subdir
    }
    else
    {
        QDir root;
        if ( !directory.isEmpty() )
        {
            root = QDir::root();

            if ( !relativeChangeDirectory( root, directory ) || !root.exists() )
            {
                cWarning() << "Working directory" << directory << "does not exist in host";
                return std::make_pair( false, QDir() );
            }
        }
        return std::make_pair( true, root );  // Now changed to subdir
    }
}

namespace Calamares
{
namespace Utils
{

Runner::Runner() {}

}  // namespace Utils
}  // namespace Calamares

Calamares::Utils::Runner::Runner( const QStringList& command )
{
    setCommand( command );
}

Calamares::Utils::Runner::~Runner() {}

Calamares::Utils::ProcessResult
Calamares::Utils::Runner::run()
{
    if ( m_command.isEmpty() )
    {
        cWarning() << "Cannot run an empty program list";
        return ProcessResult::Code::FailedToStart;
    }

    auto [ ok, workingDirectory ] = calculateWorkingDirectory( m_location, m_directory );
    if ( !ok || !workingDirectory.exists() )
    {
        // Warnings have already been printed
        return ProcessResult::Code::NoWorkingDirectory;
    }

    QProcess process;
    // Make the process run in "C" locale so we don't get issues with translation
    {
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert( "LC_ALL", "C" );
        // No guarantees that host settings for /tmp/ make sense in target
        if ( m_location == RunLocation::RunInTarget )
        {
            env.remove( "TEMP" );
            env.remove( "TEMPDIR" );
            env.remove( "TMP" );
            env.remove( "TMPDIR" );
        }
        process.setProcessEnvironment( env );
    }
    process.setProcessChannelMode( QProcess::MergedChannels );
    if ( !m_directory.isEmpty() )
    {
        process.setWorkingDirectory( workingDirectory.absolutePath() );
    }
    if ( m_location == RunLocation::RunInTarget )
    {
        process.setProgram( "chroot" );
        process.setArguments( QStringList { workingDirectory.absolutePath() } << m_command );
    }
    else
    {
        process.setProgram( "env" );
        process.setArguments( m_command );
    }

    QByteArray terminalFrameBuffer;
    const auto emitTerminalFrames = [ this, &process, &terminalFrameBuffer ]( bool flush )
    {
        terminalFrameBuffer.append( process.readAllStandardOutput() );
        while ( true )
        {
            const qsizetype carriageReturn = terminalFrameBuffer.indexOf( '\r' );
            const qsizetype newline = terminalFrameBuffer.indexOf( '\n' );
            qsizetype separator = -1;
            if ( carriageReturn >= 0 && newline >= 0 )
            {
                separator = std::min( carriageReturn, newline );
            }
            else
            {
                separator = std::max( carriageReturn, newline );
            }
            if ( separator < 0 )
            {
                break;
            }

            const QByteArray frame = terminalFrameBuffer.left( separator );
            qsizetype separatorLength = 1;
            if ( terminalFrameBuffer.at( separator ) == '\r' && separator + 1 < terminalFrameBuffer.size()
                 && terminalFrameBuffer.at( separator + 1 ) == '\n' )
            {
                separatorLength = 2;
            }
            terminalFrameBuffer.remove( 0, separator + separatorLength );
            if ( !frame.isEmpty() )
            {
                Q_EMIT this->output( QString::fromLocal8Bit( frame ) );
            }
        }
        if ( flush && !terminalFrameBuffer.isEmpty() )
        {
            Q_EMIT this->output( QString::fromLocal8Bit( terminalFrameBuffer ) );
            terminalFrameBuffer.clear();
        }
    };

    if ( m_outputMode == OutputMode::Lines )
    {
        connect( &process,
                 &QProcess::readyReadStandardOutput,
                 [ this, &process ]()
                 {
                     do
                     {
                         QString s = process.readLine();
                         if ( !s.isEmpty() )
                         {
                             Q_EMIT this->output( s );
                         }
                     } while ( process.canReadLine() );
                 } );
    }
    else if ( m_outputMode == OutputMode::TerminalFrames )
    {
        connect(
            &process, &QProcess::readyReadStandardOutput, [ &emitTerminalFrames ]() { emitTerminalFrames( false ); } );
    }

    cDebug() << Logger::SubEntry << "Running" << Logger::RedactedCommand( m_command );
    process.start();
    if ( !process.waitForStarted() )
    {
        cWarning() << "Process" << m_command.first() << "failed to start" << process.error();
        return ProcessResult::Code::FailedToStart;
    }

    if ( !m_input.isEmpty() )
    {
        process.write( m_input.toLocal8Bit() );
    }
    process.closeWriteChannel();

    bool finished = false;
    if ( m_heartbeatInterval <= std::chrono::milliseconds::zero() )
    {
        finished = process.waitForFinished(
            m_timeout > std::chrono::milliseconds::zero() ? static_cast< int >( m_timeout.count() ) : -1 );
    }
    else
    {
        QElapsedTimer elapsed;
        elapsed.start();
        const int heartbeatMilliseconds = static_cast< int >( std::max< qint64 >( 1, m_heartbeatInterval.count() ) );
        while ( process.state() != QProcess::NotRunning )
        {
            int waitMilliseconds = heartbeatMilliseconds;
            if ( m_timeout > std::chrono::milliseconds::zero() )
            {
                const qint64 remaining = m_timeout.count() - elapsed.elapsed();
                if ( remaining <= 0 )
                {
                    break;
                }
                waitMilliseconds = static_cast< int >( std::min< qint64 >( waitMilliseconds, remaining ) );
            }

            if ( process.waitForFinished( waitMilliseconds ) )
            {
                finished = true;
                break;
            }
            if ( process.state() == QProcess::NotRunning )
            {
                finished = true;
                break;
            }
            Q_EMIT heartbeat();
        }
        finished = finished || process.state() == QProcess::NotRunning;
    }

    if ( !finished )
    {
        cWarning() << "Process" << m_command.first() << "timed out after" << m_timeout.count() << "ms."
                   << Logger::NoQuote << "Output so far:\n"
                   << process.readAllStandardOutput();
        return ProcessResult::Code::TimedOut;
    }

    QString output = m_outputMode == OutputMode::Disabled
        ? QString::fromLocal8Bit( process.readAllStandardOutput() ).trimmed()
        : QString();
    if ( m_outputMode == OutputMode::Lines )
    {
        // Try to read trailing output, if any
        do
        {
            output = process.readLine();
            if ( !output.isEmpty() )
            {
                Q_EMIT this->output( output );
            }
        } while ( !output.isEmpty() );
        output = process.readAllStandardOutput();
        if ( !output.isEmpty() )
        {
            cWarning() << "Some process output left-over";
            Q_EMIT this->output( output );
        }
    }
    else if ( m_outputMode == OutputMode::TerminalFrames )
    {
        emitTerminalFrames( true );
    }

    if ( process.exitStatus() == QProcess::CrashExit )
    {
        cWarning() << "Process" << m_command.first() << "crashed." << Logger::NoQuote << "Output so far:\n" << output;
        return ProcessResult::Code::Crashed;
    }

    auto r = process.exitCode();
    const bool showDebug = ( !Calamares::Settings::instance() ) || ( Calamares::Settings::instance()->debugMode() );
    if ( r == 0 )
    {
        if ( showDebug && !output.isEmpty() )
        {
            cDebug() << Logger::SubEntry << "Finished. Exit code:" << r << Logger::NoQuote << "output:\n" << output;
        }
    }
    else  // if ( r != 0 )
    {
        if ( !output.isEmpty() )
        {
            cDebug() << Logger::SubEntry << "Target cmd:" << Logger::RedactedCommand( m_command ) << "Exit code:" << r
                     << Logger::NoQuote << "output:\n"
                     << output;
        }
        else
        {
            cDebug() << Logger::SubEntry << "Target cmd:" << Logger::RedactedCommand( m_command ) << "Exit code:" << r
                     << "(no output)";
        }
    }
    return ProcessResult( r, output );
}
