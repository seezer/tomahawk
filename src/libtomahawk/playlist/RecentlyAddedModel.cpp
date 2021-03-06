/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2012, Christian Muehlhaeuser <muesli@tomahawk-player.org>
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

#include "RecentlyAddedModel.h"

#include <QMimeData>
#include <QTreeView>

#include "Source.h"
#include "SourceList.h"
#include "database/Database.h"
#include "database/DatabaseCommand_AllTracks.h"
#include "utils/TomahawkUtils.h"
#include "utils/Logger.h"

#define LATEST_TRACK_ITEMS 250

using namespace Tomahawk;


RecentlyAddedModel::RecentlyAddedModel( const source_ptr& source, QObject* parent )
    : TrackModel( parent )
    , m_source( source )
    , m_limit( LATEST_TRACK_ITEMS )
{
    if ( source.isNull() )
    {
        connect( SourceList::instance(), SIGNAL( ready() ), SLOT( onSourcesReady() ) );
        connect( SourceList::instance(), SIGNAL( sourceAdded( Tomahawk::source_ptr ) ), SLOT( onSourceAdded( Tomahawk::source_ptr ) ) );
    }
    else
    {
        onSourceAdded( source );
        loadHistory();
    }
}


RecentlyAddedModel::~RecentlyAddedModel()
{
}


void
RecentlyAddedModel::loadHistory()
{
    if ( rowCount( QModelIndex() ) )
    {
        clear();
    }

    DatabaseCommand_AllTracks* cmd = new DatabaseCommand_AllTracks( m_source->collection() );
    cmd->setLimit( m_limit );
    cmd->setSortOrder( DatabaseCommand_AllTracks::ModificationTime );
    cmd->setSortDescending( true );

    connect( cmd, SIGNAL( tracks( QList<Tomahawk::query_ptr>, QVariant ) ),
                    SLOT( append( QList<Tomahawk::query_ptr> ) ), Qt::QueuedConnection );

    Database::instance()->enqueue( QSharedPointer<DatabaseCommand>( cmd ) );
}


void
RecentlyAddedModel::onSourcesReady()
{
    Q_ASSERT( m_source.isNull() );

    loadHistory();

    foreach ( const source_ptr& source, SourceList::instance()->sources() )
        onSourceAdded( source );
}


void
RecentlyAddedModel::onSourceAdded( const Tomahawk::source_ptr& source )
{
    connect( source->collection().data(), SIGNAL( changed() ), SLOT( loadHistory() ) );
}


bool
RecentlyAddedModel::isTemporary() const
{
    return true;
}
