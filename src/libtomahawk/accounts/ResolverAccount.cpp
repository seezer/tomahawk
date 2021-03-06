/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Leo Franchi <lfranchi@kde.org>
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

#include "ResolverAccount.h"

#include "ExternalResolver.h"
#include "ExternalResolverGui.h"
#include "AccountManager.h"
#include "TomahawkSettings.h"
#include "Source.h"

#include <Pipeline.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>

using namespace Tomahawk;
using namespace Accounts;

Account*
ResolverAccountFactory::createAccount( const QString& accountId )
{
    // Can't use this to create new accounts. Needs to be able to find account in config
    // to load proper resolver account type. Creation is done from AtticaManager when path is known
    Q_ASSERT( !accountId.isEmpty() );

    // If it's an attica resolver, return it instead so we get an icon
    const bool isFromAttica = TomahawkSettings::instance()->value( QString( "accounts/%1/atticaresolver" ).arg( accountId ), false ).toBool();
    if ( isFromAttica )
        return new AtticaResolverAccount( accountId );
    else
        return new ResolverAccount( accountId );
}


Account*
ResolverAccountFactory::createFromPath( const QString& path )
{
    return createFromPath( path, factoryId(), false );
}


Account*
ResolverAccountFactory::createFromPath( const QString& path, const QString& factory,  bool isAttica )
{
    qDebug() << "Creating ResolverAccount from path:" << path << "is attica" << isAttica;
    if ( isAttica )
    {
        QFileInfo info( path );
        return new AtticaResolverAccount( generateId( factory ), path, info.baseName() );
    }
    else
        return new ResolverAccount( generateId( factory ), path );
}


ResolverAccount::ResolverAccount( const QString& accountId )
    : Account( accountId )
{

    const QString path = configuration()[ "path" ].toString();

    // We should have a valid saved path
    Q_ASSERT( !path.isEmpty() );

    m_resolver = QWeakPointer< ExternalResolverGui >( qobject_cast< ExternalResolverGui* >( Pipeline::instance()->addScriptResolver( path, enabled() ) ) );
    connect( m_resolver.data(), SIGNAL( changed() ), this, SLOT( resolverChanged() ) );

    // What resolver do we have here? Should only be types that are 'real' resolvers
    Q_ASSERT ( !m_resolver.isNull() );

    setAccountFriendlyName( m_resolver.data()->name() );
    setTypes( AccountType( ResolverType ) );
}


ResolverAccount::ResolverAccount( const QString& accountId, const QString& path )
    : Account( accountId )
{
    QVariantHash configuration;
    configuration[ "path" ] = path;
    setConfiguration( configuration );
    setEnabled( true );

    m_resolver = QWeakPointer< ExternalResolverGui >( qobject_cast< ExternalResolverGui* >( Pipeline::instance()->addScriptResolver( path, true ) ) );
    connect( m_resolver.data(), SIGNAL( changed() ), this, SLOT( resolverChanged() ) );

    // What resolver do we have here? Should only be types that are 'real' resolvers
    Q_ASSERT ( m_resolver.data() );

    setAccountFriendlyName( m_resolver.data()->name() );
    setTypes( AccountType( ResolverType ) );

    sync();
}


ResolverAccount::~ResolverAccount()
{
    if ( m_resolver.isNull() )
        return;

    Pipeline::instance()->removeScriptResolver( m_resolver.data()->filePath() );
    delete m_resolver.data();
}



void
ResolverAccount::authenticate()
{
    Q_ASSERT( !m_resolver.isNull() );
    qDebug() << Q_FUNC_INFO << "Authenticating/starting resolver, exists?" << m_resolver;

    if ( !m_resolver.data()->running() )
        m_resolver.data()->start();

    emit connectionStateChanged( connectionState() );
}


bool
ResolverAccount::isAuthenticated() const
{
    return m_resolver.data()->running();
}


void
ResolverAccount::deauthenticate()
{
    if ( m_resolver.data()->running() )
        m_resolver.data()->stop();

    emit connectionStateChanged( connectionState() );

}


Account::ConnectionState
ResolverAccount::connectionState() const
{
    if ( m_resolver.data()->running() )
        return Connected;
    else
        return Disconnected;
}


QWidget*
ResolverAccount::configurationWidget()
{
    return m_resolver.data()->configUI();
}


QString
ResolverAccount::errorMessage() const
{
    // TODO
//     return m_resolver->error();
    return QString();
}


void
ResolverAccount::removeFromConfig()
{
    // TODO
    Account::removeFromConfig();
}


void ResolverAccount::saveConfig()
{
    Account::saveConfig();
    m_resolver.data()->saveConfig();
}


QString
ResolverAccount::path() const
{
    return m_resolver.data()->filePath();
}


void
ResolverAccount::resolverChanged()
{
    setAccountFriendlyName( m_resolver.data()->name() );
    emit connectionStateChanged( connectionState() );
}


/// AtticaResolverAccount

AtticaResolverAccount::AtticaResolverAccount( const QString& accountId )
    : ResolverAccount( accountId )
{
    TomahawkSettings::instance()->setValue( QString( "accounts/%1/atticaresolver" ).arg( accountId ), true );

    m_atticaId = configuration().value( "atticaId" ).toString();
    loadIcon();
}

AtticaResolverAccount::AtticaResolverAccount( const QString& accountId, const QString& path, const QString& atticaId )
    : ResolverAccount( accountId, path )
    , m_atticaId( atticaId )
{
    QVariantHash conf = configuration();
    conf[ "atticaId" ] = atticaId;
    setConfiguration( conf );

    TomahawkSettings::instance()->setValue( QString( "accounts/%1/atticaresolver" ).arg( accountId ), true );

    loadIcon();
    sync();
}


AtticaResolverAccount::~AtticaResolverAccount()
{

}

void
AtticaResolverAccount::loadIcon()
{
    const QFileInfo fi( m_resolver.data()->filePath() );
    QDir codeDir = fi.absoluteDir();
    codeDir.cd( "../images" );

    if ( codeDir.exists() && codeDir.exists( "icon.png" ) )
        m_icon.load( codeDir.absoluteFilePath( "icon.png" ) );

}


QPixmap
AtticaResolverAccount::icon() const
{
    return m_icon;
}
