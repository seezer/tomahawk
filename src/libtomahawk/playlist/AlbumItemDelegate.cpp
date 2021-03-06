/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *   Copyright 2011-2012, Leo Franchi            <lfranchi@kde.org>
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

#include "AlbumItemDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QAbstractItemView>
#include <QMouseEvent>

#include "Artist.h"
#include "Query.h"
#include "Result.h"
#include "Source.h"
#include "audio/AudioEngine.h"

#include "utils/TomahawkUtils.h"
#include "utils/Logger.h"
#include "utils/PixmapDelegateFader.h"
#include <utils/Closure.h>

#include "playlist/AlbumItem.h"
#include "playlist/AlbumProxyModel.h"
#include "AlbumView.h"
#include "ViewManager.h"
#include "utils/AnimatedSpinner.h"
#include "widgets/ImageButton.h"


AlbumItemDelegate::AlbumItemDelegate( QAbstractItemView* parent, AlbumProxyModel* proxy )
    : QStyledItemDelegate( (QObject*)parent )
    , m_view( parent )
    , m_model( proxy )
{
    if ( m_view && m_view->metaObject()->indexOfSignal( "modelChanged()" ) > -1 )
        connect( m_view, SIGNAL( modelChanged() ), this, SLOT( modelChanged() ) );
    
    connect( m_view, SIGNAL( scrolledContents( int, int ) ), SLOT( onScrolled( int, int ) ) );
}


QSize
AlbumItemDelegate::sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    QSize size = QStyledItemDelegate::sizeHint( option, index );
    return size;
}


void
AlbumItemDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    AlbumItem* item = m_model->sourceModel()->itemFromIndex( m_model->mapToSource( index ) );
    if ( !item )
        return;

    QStyleOptionViewItemV4 opt = option;
    initStyleOption( &opt, QModelIndex() );
    qApp->style()->drawControl( QStyle::CE_ItemViewItem, &opt, painter );

    painter->save();
    painter->setRenderHint( QPainter::Antialiasing );

/*    if ( !( option.state & QStyle::State_Selected ) )
    {
        QRect shadowRect = option.rect.adjusted( 5, 4, -5, -40 );
        painter->setPen( QColor( 90, 90, 90 ) );
        painter->drawRoundedRect( shadowRect, 0.5, 0.5 );

        QPen shadowPen( QColor( 30, 30, 30 ) );
        shadowPen.setWidth( 0.4 );
        painter->drawLine( shadowRect.bottomLeft() + QPoint( -1, 2 ), shadowRect.bottomRight() + QPoint( 1, 2 ) );

        shadowPen.setColor( QColor( 160, 160, 160 ) );
        painter->setPen( shadowPen );
        painter->drawLine( shadowRect.topLeft() + QPoint( -1, 2 ), shadowRect.bottomLeft() + QPoint( -1, 2 ) );
        painter->drawLine( shadowRect.topRight() + QPoint( 2, 2 ), shadowRect.bottomRight() + QPoint( 2, 2 ) );
        painter->drawLine( shadowRect.bottomLeft() + QPoint( 0, 3 ), shadowRect.bottomRight() + QPoint( 0, 3 ) );

        shadowPen.setColor( QColor( 180, 180, 180 ) );
        painter->setPen( shadowPen );
        painter->drawLine( shadowRect.topLeft() + QPoint( -2, 3 ), shadowRect.bottomLeft() + QPoint( -2, 1 ) );
        painter->drawLine( shadowRect.topRight() + QPoint( 3, 3 ), shadowRect.bottomRight() + QPoint( 3, 1 ) );
        painter->drawLine( shadowRect.bottomLeft() + QPoint( 0, 4 ), shadowRect.bottomRight() + QPoint( 0, 4 ) );
    }*/

//    QRect r = option.rect.adjusted( 6, 5, -6, -41 );
    QRect r = option.rect;

    QString top, bottom;
    if ( !item->album().isNull() )
    {
        top = item->album()->name();

        if ( !item->album()->artist().isNull() )
            bottom = item->album()->artist()->name();
    }
    else if ( !item->artist().isNull() )
    {
        top = item->artist()->name();
    }
    else
    {
        top = item->query()->track();
        bottom = item->query()->artist();
    }

    if ( !m_covers.contains( index ) )
    {
        if ( !item->album().isNull() )
        {
            m_covers.insert( index, QSharedPointer< Tomahawk::PixmapDelegateFader >( new Tomahawk::PixmapDelegateFader( item->album(), r.size(), TomahawkUtils::Grid ) ) );
        }
        else if ( !item->artist().isNull() )
        {
            m_covers.insert( index, QSharedPointer< Tomahawk::PixmapDelegateFader >( new Tomahawk::PixmapDelegateFader( item->artist(), r.size(), TomahawkUtils::Grid ) ) );
        }
        else
        {
            m_covers.insert( index, QSharedPointer< Tomahawk::PixmapDelegateFader >( new Tomahawk::PixmapDelegateFader( item->query(), r.size(), TomahawkUtils::Grid ) ) );
        }

        _detail::Closure* closure = NewClosure( m_covers[ index ], SIGNAL( repaintRequest() ), const_cast<AlbumItemDelegate*>(this), SLOT( doUpdateIndex( QPersistentModelIndex ) ), QPersistentModelIndex( index ) );
        closure->setAutoDelete( false );
    }

    QSharedPointer< Tomahawk::PixmapDelegateFader > fader = m_covers[ index ];
    if ( fader->size() != r.size() )
        fader->setSize( r.size() );

    const QPixmap cover = fader->currentPixmap();

    if ( false && option.state & QStyle::State_Selected )
    {
#if defined(Q_WS_MAC) || defined(Q_WS_WIN)
        painter->save();

        QPainterPath border;
        border.addRoundedRect( r.adjusted( -2, -2, 2, 2 ), 3, 3 );
        QPen borderPen( QColor( 86, 170, 243 ) );
        borderPen.setWidth( 5 );
        painter->setPen( borderPen );
        painter->drawPath( border );

        painter->restore();
#else
        opt.palette.setColor( QPalette::Text, opt.palette.color( QPalette::HighlightedText ) );
#endif
    }

    painter->drawPixmap( r, cover );

    if ( m_hoverIndex == index )
    {
        painter->save();

        painter->setPen( QColor( 33, 33, 33 ) );
        painter->setBrush( QColor( 33, 33, 33 ) );
        painter->setOpacity( 0.5 );
        painter->drawRect( r );

        painter->restore();
    }

    painter->save();

    painter->setPen( Qt::black );
    painter->setBrush( Qt::black );
    painter->setOpacity( 0.5 );
    painter->drawRoundedRect( r.adjusted( 4, +r.height() - 36, -4, -4 ), 3, 3 );

    painter->restore();

    painter->setPen( opt.palette.color( QPalette::HighlightedText ) );
    QTextOption to;
    to.setWrapMode( QTextOption::NoWrap );

    QString text;
    QFont font = opt.font;
    font.setPixelSize( 10 );
    QFont boldFont = font;
    boldFont.setBold( true );
    boldFont.setPixelSize( 14 );

    QRect textRect = option.rect.adjusted( 6, option.rect.height() - 36, -4, -6 );
    painter->setFont( font );
    int bottomHeight = painter->fontMetrics().boundingRect( bottom ).height();
    painter->setFont( boldFont );
    int topHeight = painter->fontMetrics().boundingRect( top ).height();

    bool oneLiner = false;
    if ( bottom.isEmpty() )
        oneLiner = true;
    else
        oneLiner = ( textRect.height() < topHeight + bottomHeight );

    if ( oneLiner )
    {
        to.setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        text = painter->fontMetrics().elidedText( top, Qt::ElideRight, textRect.width() - 3 );
        painter->drawText( textRect, text, to );
    }
    else
    {
        to.setAlignment( Qt::AlignHCenter | Qt::AlignTop );
        text = painter->fontMetrics().elidedText( top, Qt::ElideRight, textRect.width() - 3 );
        painter->drawText( textRect, text, to );

        painter->setFont( font );
        // If the user is hovering over an artist rect, draw a background so she knows it's clickable
        QRect r = textRect;
        r.setTop( r.bottom() - painter->fontMetrics().height() );
        r.adjust( 4, 0, -4, -1 );
        if ( m_hoveringOver == index )
        {
            TomahawkUtils::drawQueryBackground( painter, opt.palette, r, 1.1 );
            painter->setPen( opt.palette.color( QPalette::HighlightedText ) );
        }
        else
        {
/*            if ( !( option.state & QStyle::State_Selected ) )
#ifdef Q_WS_MAC
                painter->setPen( opt.palette.color( QPalette::Dark ).darker( 200 ) );
#else
                painter->setPen( opt.palette.color( QPalette::Dark ) );
#endif*/
        }

        to.setAlignment( Qt::AlignHCenter | Qt::AlignBottom );
        text = painter->fontMetrics().elidedText( bottom, Qt::ElideRight, textRect.width() - 10 );
        painter->drawText( textRect.adjusted( 5, -1, -5, -1 ), text, to );

        // Calculate rect of artist on-hover button click area
        m_artistNameRects[ index ] = r;
    }

    painter->restore();
}


void
AlbumItemDelegate::onPlayClicked( const QPersistentModelIndex& index )
{
    QPoint pos = m_playButton[ index ]->pos();
    foreach ( ImageButton* button, m_playButton )
        button->deleteLater();
    m_playButton.clear();

    AnimatedSpinner* spinner = new AnimatedSpinner( m_view );
    spinner->setAutoCenter( false );
    spinner->fadeIn();
    spinner->move( pos );

    m_spinner[ index ] = spinner;
    
    AlbumItem* item = m_model->sourceModel()->itemFromIndex( m_model->mapToSource( index ) );
    if ( item )
    {
        _detail::Closure* closure;
        
        closure = NewClosure( AudioEngine::instance(), SIGNAL( loading( Tomahawk::result_ptr ) ),
                              const_cast<AlbumItemDelegate*>(this), SLOT( onPlaybackStarted( QPersistentModelIndex ) ), QPersistentModelIndex( index ) );

        closure = NewClosure( AudioEngine::instance(), SIGNAL( started( Tomahawk::result_ptr ) ),
                              const_cast<AlbumItemDelegate*>(this), SLOT( onPlaylistChanged( QPersistentModelIndex ) ), QPersistentModelIndex( index ) );
        closure->setAutoDelete( false );

        connect( AudioEngine::instance(), SIGNAL( stopped() ), SLOT( onPlaybackFinished() ) );

        if ( !item->query().isNull() )
            AudioEngine::instance()->playItem( Tomahawk::playlistinterface_ptr(), item->query() );
        else if ( !item->album().isNull() )
            AudioEngine::instance()->playItem( item->album() );
        else if ( !item->artist().isNull() )
            AudioEngine::instance()->playItem( item->artist() );
    }
}


bool
AlbumItemDelegate::editorEvent( QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index )
{
    Q_UNUSED( model );
    Q_UNUSED( option );

    if ( event->type() != QEvent::MouseButtonRelease &&
         event->type() != QEvent::MouseMove &&
         event->type() != QEvent::MouseButtonPress &&
         event->type() != QEvent::Leave )
        return false;

    bool hoveringArtist = false;
    if ( m_artistNameRects.contains( index ) )
    {
        QRect artistNameRect = m_artistNameRects[ index ];
        QMouseEvent* ev = static_cast< QMouseEvent* >( event );
        hoveringArtist = artistNameRect.contains( ev->pos() );
    }

    if ( event->type() == QEvent::MouseMove )
    {
        foreach ( const QModelIndex& idx, m_playButton.keys() )
        {
            if ( index != idx )
                m_playButton.take( idx )->deleteLater();
        }
        
        if ( !m_playButton.contains( index ) && !m_spinner.contains( index ) && !m_pauseButton.contains( index ) )
        {
            foreach ( ImageButton* button, m_playButton )
                button->deleteLater();
            m_playButton.clear();

            ImageButton* button = new ImageButton( m_view );
            button->setPixmap( RESPATH "images/play-rest.png" );
            button->setPixmap( RESPATH "images/play-pressed.png", QIcon::Off, QIcon::Active );
            button->setFixedSize( 48, 48 );
            button->move( option.rect.center() - QPoint( 23, 23 ) );
            button->setContentsMargins( 0, 0, 0, 0 );
            button->show();
            
            NewClosure( button, SIGNAL( clicked( bool ) ),
                        const_cast<AlbumItemDelegate*>(this), SLOT( onPlayClicked( QPersistentModelIndex ) ), QPersistentModelIndex( index ) );

            m_playButton[ index ] = button;
        }

        if ( m_hoveringOver != index || ( !hoveringArtist && m_hoveringOver.isValid() ) )
        {
            emit updateIndex( m_hoveringOver );

            if ( hoveringArtist )
                m_hoveringOver = index;
            else
                m_hoveringOver = QPersistentModelIndex();

            emit updateIndex( index );
        }

        if ( m_hoverIndex != index )
        {
            emit updateIndex( m_hoverIndex );
            m_hoverIndex = index;
            emit updateIndex( index );
        }
        
        event->accept();
        return true;
    }

    if ( hoveringArtist )
    {
        if ( event->type() == QEvent::MouseButtonRelease )
        {
            AlbumItem* item = m_model->sourceModel()->itemFromIndex( m_model->mapToSource( index ) );
            if ( !item )
                return false;

            if ( !item->query().isNull() )
                ViewManager::instance()->show( Tomahawk::Artist::get( item->query()->artist() ) );
            else if ( !item->album().isNull() && !item->album()->artist().isNull() )
                ViewManager::instance()->show( item->album()->artist() );

            event->accept();
            return true;
        }
        else if ( event->type() == QEvent::MouseButtonPress )
        {
            // Stop the whole album from having a down click action as we just want the artist name to be clicked
            event->accept();
            return true;
        }
    }

    return false;
}


void
AlbumItemDelegate::modelChanged()
{
    m_artistNameRects.clear();
    m_hoveringOver = QPersistentModelIndex();
    m_hoverIndex = QPersistentModelIndex();

    foreach ( ImageButton* button, m_playButton )
        button->deleteLater();
    m_playButton.clear();
    foreach ( ImageButton* button, m_pauseButton )
        button->deleteLater();
    m_pauseButton.clear();
    foreach ( QWidget* widget, m_spinner )
        widget->deleteLater();
    m_spinner.clear();

    if ( AlbumView* view = qobject_cast< AlbumView* >( m_view ) )
        m_model = view->proxyModel();
}


void
AlbumItemDelegate::doUpdateIndex( const QPersistentModelIndex& idx )
{
    if ( !idx.isValid() )
        return;
    emit updateIndex( idx );
}


void
AlbumItemDelegate::onScrolled( int dx, int dy )
{
    foreach ( QWidget* widget, m_spinner.values() )
    {
        widget->move( widget->pos() + QPoint( dx, dy ) );
    }
    foreach ( ImageButton* button, m_playButton.values() )
    {
        button->move( button->pos() + QPoint( dx, dy ) );
    }
    foreach ( ImageButton* button, m_pauseButton.values() )
    {
        button->move( button->pos() + QPoint( dx, dy ) );
    }
}


void
AlbumItemDelegate::onPlaybackFinished()
{
    foreach ( ImageButton* button, m_pauseButton )
        button->deleteLater();
    m_pauseButton.clear();
}


void
AlbumItemDelegate::onPlaylistChanged( const QPersistentModelIndex& index )
{
    AlbumItem* item = m_model->sourceModel()->itemFromIndex( m_model->mapToSource( index ) );
    if ( item )
    {
        bool finished = false;

        if ( !item->query().isNull() )
        {
            if ( !item->query()->numResults() || AudioEngine::instance()->currentTrack() != item->query()->results().first() )
                finished = true;
        }
        else if ( !item->album().isNull() )
        {
            if ( AudioEngine::instance()->currentTrackPlaylist() != item->album()->playlistInterface( Tomahawk::Mixed ) )
                finished = true;
        }
        else if ( !item->artist().isNull() )
        {
            if ( AudioEngine::instance()->currentTrackPlaylist() != item->artist()->playlistInterface( Tomahawk::Mixed ) )
                finished = true;
        }
        
        if ( finished )
        {
            if ( m_pauseButton.contains( index ) )
            {
                m_pauseButton[ index ]->deleteLater();
                m_pauseButton.remove( index );
            }
        }
    }
}


void
AlbumItemDelegate::onPlaybackStarted( const QPersistentModelIndex& index )
{
    if ( !m_spinner.contains( index ) )
        return;

    QPoint pos = m_spinner[ index ]->pos();
    foreach ( QWidget* widget, m_spinner.values() )
    {
        delete widget;
    }
    m_spinner.clear();
    
    ImageButton* button = new ImageButton( m_view );
    button->setPixmap( RESPATH "images/pause-rest.png" );
    button->setPixmap( RESPATH "images/pause-pressed.png", QIcon::Off, QIcon::Active );
    button->setFixedSize( 48, 48 );
    button->move( pos );
    button->setContentsMargins( 0, 0, 0, 0 );
    button->show();
            
    connect( button, SIGNAL( clicked( bool ) ), AudioEngine::instance(), SLOT( playPause() ) );

    m_pauseButton[ index ] = button;
}
