/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *   Copyright 2010-2012, Jeff Mitchell <jeff@tomahawk-player.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Source.h"

#include "Collection.h"
#include "SourceList.h"
#include "SourcePlaylistInterface.h"

#include "network/ControlConnection.h"
#include "database/DatabaseCommand_AddSource.h"
#include "database/DatabaseCommand_CollectionStats.h"
#include "database/DatabaseCommand_SourceOffline.h"
#include "database/DatabaseCommand_UpdateSearchIndex.h"
#include "database/Database.h"

#include <QCoreApplication>
#include <QBuffer>

#include "utils/Logger.h"
#include "utils/TomahawkUtilsGui.h"
#include "utils/TomahawkCache.h"
#include "database/DatabaseCommand_SocialAction.h"

using namespace Tomahawk;


Source::Source( int id, const QString& username )
    : QObject()
    , m_isLocal( false )
    , m_online( false )
    , m_username( username )
    , m_id( id )
    , m_updateIndexWhenSynced( false )
    , m_state( DBSyncConnection::UNKNOWN )
    , m_cc( 0 )
    , m_commandCount( 0 )
    , m_avatar( 0 )
    , m_fancyAvatar( 0 )
{
    m_scrubFriendlyName = qApp->arguments().contains( "--demo" );

    if ( id == 0 )
    {
        m_isLocal = true;
        m_online = true;
    }

    m_currentTrackTimer.setSingleShot( true );
    connect( &m_currentTrackTimer, SIGNAL( timeout() ), this, SLOT( trackTimerFired() ) );
}


Source::~Source()
{
    qDebug() << Q_FUNC_INFO << friendlyName();
    delete m_avatar;
    delete m_fancyAvatar;
}


void
Source::setControlConnection( ControlConnection* cc )
{
    m_cc = cc;
    if ( cc )
        connect( cc, SIGNAL( finished() ), SLOT( setOffline() ), Qt::QueuedConnection );
}


collection_ptr
Source::collection() const
{
    if( m_collections.length() )
        return m_collections.first();

    collection_ptr tmp;
    return tmp;
}


void
Source::setStats( const QVariantMap& m )
{
    m_stats = m;
    emit stats( m_stats );
    emit stateChanged();
}


QString
Source::friendlyName() const
{
    if ( m_friendlyname.isEmpty() )
        return m_username;

    //TODO: this is a terrible assumption, help me clean this up, mighty muesli!
    if ( m_friendlyname.contains( "@conference." ) )
        return QString( m_friendlyname ).remove( 0, m_friendlyname.lastIndexOf( "/" ) + 1 ).append( " via MUC" );

    if ( m_friendlyname.contains( "/" ) )
        return m_friendlyname.left( m_friendlyname.indexOf( "/" ) );

    return m_friendlyname;
}


#ifndef ENABLE_HEADLESS
void
Source::setAvatar( const QPixmap& avatar )
{
    delete m_avatar;
    m_avatar = new QPixmap( avatar );
    m_fancyAvatar = 0;

    QByteArray ba;
    QBuffer buffer( &ba );
    buffer.open( QIODevice::WriteOnly );
    avatar.save( &buffer, "PNG" );

    tDebug() << Q_FUNC_INFO << friendlyName() << m_username << ba.count();
    TomahawkUtils::Cache::instance()->putData( "Sources", 7776000000 /* 90 days */, m_username, ba );
}


QPixmap
Source::avatar( AvatarStyle style, const QSize& size ) const
{
    if ( !m_avatar )
    {
        m_avatar = new QPixmap();
        QByteArray ba = TomahawkUtils::Cache::instance()->getData( "Sources", m_username ).toByteArray();

        if ( ba.count() )
            m_avatar->loadFromData( ba );
        if ( m_avatar->isNull() )
        {
            delete m_avatar;
            m_avatar = 0;
        }
    }

    if ( style == FancyStyle && m_avatar && !m_fancyAvatar )
        m_fancyAvatar = new QPixmap( TomahawkUtils::createAvatarFrame( QPixmap( *m_avatar ) ) );

    QPixmap pixmap;
    if ( style == Original && m_avatar )
        pixmap = *m_avatar;
    else if ( style == FancyStyle && m_fancyAvatar )
        pixmap = *m_fancyAvatar;

    if ( !pixmap.isNull() && !size.isEmpty() )
    {
        if ( m_coverCache.contains( size.width() ) )
        {
            return m_coverCache.value( size.width() );
        }

        QPixmap scaledCover;
        scaledCover = pixmap.scaled( size, Qt::KeepAspectRatio, Qt::SmoothTransformation );
        m_coverCache.insert( size.width(), scaledCover );
        return scaledCover;
    }

    return pixmap;
}
#endif


void
Source::setFriendlyName( const QString& fname )
{
    if ( fname.isEmpty() )
        return;

    m_friendlyname = fname;
    if ( m_scrubFriendlyName )
        m_friendlyname = m_friendlyname.split( "@" ).first();
}


void
Source::addCollection( const collection_ptr& c )
{
    Q_ASSERT( m_collections.length() == 0 ); // only 1 source supported atm
    m_collections.append( c );
    emit collectionAdded( c );
}


void
Source::removeCollection( const collection_ptr& c )
{
    Q_ASSERT( m_collections.length() == 1 && m_collections.first() == c ); // only 1 source supported atm
    m_collections.removeAll( c );
    emit collectionRemoved( c );
}


void
Source::setOffline()
{
    qDebug() << Q_FUNC_INFO << friendlyName();
    if ( !m_online )
        return;

    m_online = false;
    emit offline();

    m_currentTrack.clear();
    emit stateChanged();

    m_cc = 0;
    DatabaseCommand_SourceOffline* cmd = new DatabaseCommand_SourceOffline( id() );
    Database::instance()->enqueue( QSharedPointer< DatabaseCommand >( cmd ) );
}


void
Source::setOnline()
{
    qDebug() << Q_FUNC_INFO << friendlyName();
    if ( m_online )
        return;

    m_online = true;
    emit online();

    // ensure username is in the database
    DatabaseCommand_addSource* cmd = new DatabaseCommand_addSource( m_username, friendlyName() );
    connect( cmd, SIGNAL( done( unsigned int, QString ) ),
                    SLOT( dbLoaded( unsigned int, const QString& ) ) );
    Database::instance()->enqueue( QSharedPointer<DatabaseCommand>(cmd) );
}


void
Source::dbLoaded( unsigned int id, const QString& fname )
{
    m_id = id;
    setFriendlyName( fname );

    emit syncedWithDatabase();
}


void
Source::scanningProgress( unsigned int files )
{
    if ( files )
        m_textStatus = tr( "Scanning (%L1 tracks)" ).arg( files );
    else
        m_textStatus = tr( "Scanning" );

    emit stateChanged();
}


void
Source::scanningFinished( unsigned int files )
{
    Q_UNUSED( files );

    m_textStatus = QString();

    if ( m_updateIndexWhenSynced )
    {
        m_updateIndexWhenSynced = false;
        updateTracks();
    }

    emit stateChanged();
    emit synced();
}


void
Source::onStateChanged( DBSyncConnection::State newstate, DBSyncConnection::State oldstate, const QString& info )
{
    Q_UNUSED( oldstate );

    QString msg;
    switch( newstate )
    {
        case DBSyncConnection::CHECKING:
        {
            msg = tr( "Checking" );
            break;
        }
        case DBSyncConnection::FETCHING:
        {
            msg = tr( "Fetching" );
            break;
        }
        case DBSyncConnection::PARSING:
        {
            msg = tr( "Parsing" );
            break;
        }
        case DBSyncConnection::SCANNING:
        {
            msg = tr( "Scanning (%L1 tracks)" ).arg( info );
            break;
        }
        case DBSyncConnection::SYNCED:
        {
            msg = QString();
            break;
        }

        default:
            msg = QString();
    }

    m_state = newstate;
    m_textStatus = msg;
    emit stateChanged();
}


unsigned int
Source::trackCount() const
{
    return m_stats.value( "numfiles" ).toUInt();
}


Tomahawk::playlistinterface_ptr
Source::playlistInterface()
{
    if ( m_playlistInterface.isNull() )
    {
        Tomahawk::source_ptr source = SourceList::instance()->get( id() );
        m_playlistInterface = Tomahawk::playlistinterface_ptr( new Tomahawk::SourcePlaylistInterface( source.data() ) );
    }

    return m_playlistInterface;
}


void
Source::onPlaybackStarted( const Tomahawk::query_ptr& query, unsigned int duration )
{
    qDebug() << Q_FUNC_INFO << query->toString();

    m_currentTrack = query;
    m_currentTrackTimer.start( duration * 1000 + 900000 ); // duration comes in seconds

    if ( m_playlistInterface.isNull() )
        playlistInterface();
    emit playbackStarted( query );
}


void
Source::onPlaybackFinished( const Tomahawk::query_ptr& query )
{
    qDebug() << Q_FUNC_INFO << query->toString();
    emit playbackFinished( query );

    m_currentTrackTimer.start();
}


void
Source::trackTimerFired()
{
    m_currentTrack.clear();

    emit stateChanged();
}


void
Source::addCommand( const QSharedPointer<DatabaseCommand>& command )
{
    if ( QThread::currentThread() != thread() )
    {
        QMetaObject::invokeMethod( this, "addCommand", Qt::QueuedConnection, Q_ARG( const QSharedPointer<DatabaseCommand>, command ) );
        return;
    }

    m_cmds << command;
    if ( !command->singletonCmd() )
        m_lastCmdGuid = command->guid();

    m_commandCount = m_cmds.count();
}


void
Source::executeCommands()
{
    if ( QThread::currentThread() != thread() )
    {
        QMetaObject::invokeMethod( this, "executeCommands", Qt::QueuedConnection );
        return;
    }

    if ( !m_cmds.isEmpty() )
    {
        QList< QSharedPointer<DatabaseCommand> > cmdGroup;
        QSharedPointer<DatabaseCommand> cmd = m_cmds.takeFirst();
        while ( cmd->groupable() )
        {
            cmdGroup << cmd;
            if ( !m_cmds.isEmpty() && m_cmds.first()->groupable() && m_cmds.first()->commandname() == cmd->commandname() )
                cmd = m_cmds.takeFirst();
            else
                break;
        }

        // return here when the last command finished
        connect( cmd.data(), SIGNAL( finished() ), SLOT( executeCommands() ) );

        if ( cmdGroup.count() )
        {
            Database::instance()->enqueue( cmdGroup );
        }
        else
        {
            Database::instance()->enqueue( cmd );
        }

        int percentage = ( float( m_commandCount - m_cmds.count() ) / (float)m_commandCount ) * 100.0;
        m_textStatus = tr( "Saving (%1%)" ).arg( percentage );
        emit stateChanged();
    }
    else
    {
        if ( m_updateIndexWhenSynced )
        {
            m_updateIndexWhenSynced = false;
            updateTracks();
        }

        m_textStatus = QString();
        m_state = DBSyncConnection::SYNCED;

        emit commandsFinished();
        emit stateChanged();
        emit synced();
    }
}


void
Source::reportSocialAttributesChanged( DatabaseCommand_SocialAction* action )
{
    Q_ASSERT( action );

    emit socialAttributesChanged( action->action() );

    if ( action->action() == "latchOn" )
    {
        const source_ptr to = SourceList::instance()->get( action->comment() );
        if ( !to.isNull() )
            emit latchedOn( to );
    }
    else if ( action->action() == "latchOff" )
    {
        const source_ptr from = SourceList::instance()->get( action->comment() );
        if ( !from.isNull() )
            emit latchedOff( from );
    }
}


void
Source::updateTracks()
{
    {
        DatabaseCommand* cmd = new DatabaseCommand_UpdateSearchIndex();
        Database::instance()->enqueue( QSharedPointer<DatabaseCommand>( cmd ) );
    }

    {
        // Re-calculate local db stats
        DatabaseCommand_CollectionStats* cmd = new DatabaseCommand_CollectionStats( SourceList::instance()->get( id() ) );
        connect( cmd, SIGNAL( done( QVariantMap ) ), SLOT( setStats( QVariantMap ) ), Qt::QueuedConnection );
        Database::instance()->enqueue( QSharedPointer<DatabaseCommand>( cmd ) );
    }
}


void
Source::updateIndexWhenSynced()
{
    m_updateIndexWhenSynced = true;
}
