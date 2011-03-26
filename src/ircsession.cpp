/*
* Copyright (C) 2008-2011 J-P Nurmi <jpnurmi@gmail.com>
* Copyright (C) 2010-2011 SmokeX smokexjc@gmail.com
*
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This library is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
* License for more details.
*/

#include "ircsession.h"
#include "ircsession_p.h"
#include "ircbuffer.h"
#include "ircbuffer_p.h"
#include "ircmessage.h"
#include "ircutil.h"
#include <QBuffer>
#include <QPointer>
#include <QTcpSocket>
#include <QStringList>
#include <QMetaMethod>

/*!
    \class IrcSession ircsession.h
    \brief The IrcSession class provides an IRC session.

    IRC (Internet Relay Chat protocol) is a simple communication protocol.
    IrcSession provides means to establish a connection to an IRC server.
    IrcSession works asynchronously. None of the functions block the
    calling thread but they return immediately and the actual work is done
    behind the scenes in the event loop.

    Example usage:
    \code
    IrcSession* session = new IrcSession(this);
    session->setNick("jpnurmi");
    session->setIdent("jpn");
    session->setRealName("J-P Nurmi");
    session->connectToServer("irc.freenode.net", 6667);
    \endcode

    \note IrcSession supports SSL (Secure Sockets Layer) connections since version 0.3.0

    Example SSL usage:
    \code
    IrcSession* session = new IrcSession(this);
    // ...
    QSslSocket* socket = new QSslSocket(session);
    socket->ignoreSslErrors();
    socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    session->setSocket(socket);
    session->connectToServer("irc.secure.ssl", 6669);
    \endcode

    \sa setSocket()
 */

/*!
    \fn void IrcSession::connecting()

    This signal is emitted when the connection is being established.
 */

/*!
    \fn void IrcSession::connected()

    This signal is emitted when the welcome message has been received.

    \sa Irc::RPL_WELCOME
 */

/*!
    \fn void IrcSession::disconnected()

    This signal is emitted when the session has been disconnected.
 */

IrcSessionPrivate::IrcSessionPrivate(IrcSession* session) :
    q_ptr(session),
    parser(),
    buffer(),
    socket(0),
    host(),
    port(6667),
    userName(),
    nickName(),
    realName(),
    mainBuffer(0),
    buffers()
{
}

void IrcSessionPrivate::_q_connected()
{
    Q_Q(IrcSession);
    emit q->connecting();

    QString password;
    emit q->password(&password);
    if (!password.isEmpty())
        socket->write(QString(QLatin1String("PASS %1\r\n")).arg(password).toLocal8Bit());

    socket->write(QString(QLatin1String("NICK %1\r\n")).arg(nickName).toLocal8Bit());

    // RFC 1459 states that "hostname and servername are normally
    // ignored by the IRC server when the USER command comes from
    // a directly connected client (for security reasons)", therefore
    // we don't need them.
    socket->write(QString(QLatin1String("USER %1 unknown unknown :%2\r\n")).arg(userName, realName).toLocal8Bit());

    mainBuffer = q->createBuffer(QLatin1String("*"));
}

void IrcSessionPrivate::_q_disconnected()
{
    Q_Q(IrcSession);
    emit q->disconnected();
}

void IrcSessionPrivate::_q_reconnect()
{
    if (socket)
    {
        socket->connectToHost(host, port);
        if (socket->inherits("QSslSocket"))
            QMetaObject::invokeMethod(socket, "startClientEncryption");
    }
}

void IrcSessionPrivate::_q_error(QAbstractSocket::SocketError error)
{
    qDebug() << "_q_error" << error;
}

void IrcSessionPrivate::_q_state(QAbstractSocket::SocketState state)
{
    qDebug() << "_q_state" << state;
}

void IrcSessionPrivate::_q_readData()
{
    buffer += socket->readAll();
    // try reading RFC compliant message lines first
    readLines("\r\n");
    // fall back to RFC incompliant lines...
    readLines("\n");
}

void IrcSessionPrivate::readLines(const QByteArray& delimiter)
{
    int i = -1;
    while ((i = buffer.indexOf(delimiter)) != -1)
    {
        QByteArray line = buffer.left(i).trimmed();
        buffer = buffer.mid(i + delimiter.length());
        if (!line.isEmpty())
            processLine(line);
    }
}

void IrcSessionPrivate::processLine(const QByteArray& line)
{
    Q_Q(IrcSession);
    parser.parse(line);

    qDebug() << line;

    QString prefix = parser.prefix();
    QString command = parser.command();
    QStringList params = parser.params();

    bool isNumeric = false;
    uint code = command.toInt(&isNumeric);

    // handle PING/PONG
    if (command == QLatin1String("PING"))
    {
        QString arg = params.value(0);
        raw(QString(QLatin1String("PONG %1")).arg(arg));
        return;
    }

    // and dump
    if (isNumeric)
    {
        switch (code)
        {
            case Irc::RPL_WELCOME:
            {
                emit q->connected();
                qDebug("WELCOME");
                break;
            }

            /* TODO:
            case Irc::RPL_TOPIC:
            {
                QString channel = params.value(1);
                QString target = resolveTarget(QString(), channel);
                createBuffer(target);
                break;
            }

            case Irc::RPL_NAMREPLY:
            {
                QStringList list = params;
                list.removeAll(QLatin1String("="));
                list.removeAll(QLatin1String("@"));
                list.removeAll(QLatin1String("*"));

                QString target = resolveTarget(QString(), list.value(1));
                IrcBuffer* buffer = createBuffer(target);
                buffer->d_func()->names += list.value(2).split(QLatin1String(" "), QString::SkipEmptyParts);
                break;
            }

            case Irc::RPL_ENDOFNAMES:
            {
                QString target = resolveTarget(QString(), params.value(1));
                IrcBuffer* buffer = createBuffer(target);
                emit buffer->namesReceived(buffer->d_func()->names);
                break;
            }

            case Irc::RPL_MOTDSTART:
                motd.clear();
                break;

            case Irc::RPL_MOTD:
                motd.append(params.value(1) + QLatin1Char('\n'));
                break;

            case Irc::RPL_ENDOFMOTD:
                if (defaultBuffer)
                    emit defaultBuffer->motdReceived(motd);
                motd.clear();
                break;
            */

            default:
                break;
        }

        //if (defaultBuffer)
        //    emit defaultBuffer->numericMessageReceived(prefix, code, params);
    }
    else
    {
        if (command == QLatin1String("JOIN"))
        {
            //IrcJoinMessage msg = IrcJoinMessage::fromString(params.join(" "));
            // ...
        }
    }

    /*else
    {
        if (command == QLatin1String("NICK"))
        {
            QString oldNick = Util::nickFromTarget(prefix);
            QString newNick = params.value(0);

            if (nick == oldNick)
                nick = newNick;

            foreach (IrcBuffer* buffer, buffers)
            {
                if (buffer->receiver() == oldNick)
                    buffer->d_func()->setReceiver(newNick);

                if (buffer->names().contains(oldNick))
                {
                    buffer->d_func()->removeName(oldNick);
                    buffer->d_func()->addName(newNick);
                    emit buffer->nickChanged(prefix, newNick);
                }
            }
        }
        else if (command == QLatin1String("QUIT"))
        {
            QString reason = params.value(0);
            QString nick = Util::nickFromTarget(prefix);

            foreach (IrcBuffer* buffer, buffers)
            {
                if (buffer->names().contains(nick))
                {
                    buffer->d_func()->removeName(nick);
                    emit buffer->quit(prefix, reason);
                }
            }
        }
        else if (command == QLatin1String("JOIN"))
        {
            QString channel = params.value(0);
            QString target = resolveTarget(prefix, channel);
            IrcBuffer* buffer = createBuffer(target);
            //buffer->d_func()->addName(Util::nickFromTarget(prefix));
            emit buffer->joined(prefix);
        }
        else if (command == QLatin1String("PART"))
        {
            QString channel = params.value(0);
            QString target = resolveTarget(prefix, channel);
            if (nick != Util::nickFromTarget(prefix))
            {
                QString message = params.value(1);
                IrcBuffer* buffer = createBuffer(target);
                //buffer->d_func()->removeName(Util::nickFromTarget(prefix));
                emit buffer->parted(prefix, message);
            }
            else if (buffers.contains(target.toLower()))
            {
                IrcBuffer* buffer = buffers.value(target.toLower());
                removeBuffer(buffer);
                buffer->deleteLater();
            }
        }
        else if (command == QLatin1String("MODE"))
        {
            QString receiver = params.value(0);
            QString mode = params.value(1);
            QString args = params.value(2);
            QString target = resolveTarget(prefix, receiver);
            IrcBuffer* buffer = createBuffer(target);
            //buffer->d_func()->updateMode(args, mode);
            emit buffer->modeChanged(prefix, mode, args);
        }
        else if (command == QLatin1String("TOPIC"))
        {
            QString channel = params.value(0);
            QString topic = params.value(1);
            QString target = resolveTarget(prefix, channel);
            IrcBuffer* buffer = createBuffer(target);
            //buffer->d_func()->topic = topic;
            emit buffer->topicChanged(prefix, topic);
        }
        else if (command == QLatin1String("INVITE"))
        {
            QString receiver = params.value(0);
            QString channel = params.value(1);
            if (defaultBuffer)
                emit defaultBuffer->invited(prefix, receiver, channel);
        }
        else if (command == QLatin1String("KICK"))
        {
            QString channel = params.value(0);
            QString nick = params.value(1);
            QString message = params.value(2);
            QString target = resolveTarget(prefix, channel);
            IrcBuffer* buffer = createBuffer(target);
            //buffer->d_func()->removeName(nick);
            emit buffer->kicked(prefix, nick, message);
        }
        else if (command == QLatin1String("PRIVMSG"))
        {
            QString message = params.value(1);

            if (message.startsWith(QLatin1Char('\1')) && message.endsWith(QLatin1Char('\1')))
            {
                message.remove(0, 1);
                message.remove(message.length() - 1, 1);

                if (message.startsWith(QLatin1String("ACTION ")))
                {
                    QString receiver = params.value(0);
                    QString target = resolveTarget(prefix, receiver);
                    IrcBuffer* buffer = createBuffer(target);
                    emit buffer->ctcpActionReceived(prefix, message.mid(7));
                }
                else
                {
                    if (defaultBuffer)
                        emit defaultBuffer->ctcpRequestReceived(prefix, message);
                }
            }
            else
            {
                QString receiver = params.value(0);
                QString target = resolveTarget(prefix, receiver);
                IrcBuffer* buffer = createBuffer(target);
                emit buffer->messageReceived(prefix, message);
            }
        }
        else if (command == QLatin1String("NOTICE"))
        {
            QString receiver = params.value(0);
            QString message = params.value(1);

            if (message.startsWith(QLatin1Char('\1')) && message.endsWith(QLatin1Char('\1')))
            {
                message.remove(0, 1);
                message.remove(message.length() - 1, 1);

                if (defaultBuffer)
                    emit defaultBuffer->ctcpReplyReceived(prefix, message);
            }
            else
            {
                QString target = resolveTarget(prefix, receiver);
                IrcBuffer* buffer = createBuffer(target);
                emit buffer->noticeReceived(prefix, message);
            }
        }
        else if (command == QLatin1String("KILL"))
        {
            ; // ignore this event - not all servers generate this
        }
        else
        {
            // The "unknown" event is triggered upon receipt of any number of
            // unclassifiable miscellaneous messages, which aren't handled by
            // the library.
            if (defaultBuffer)
                emit defaultBuffer->unknownMessageReceived(prefix, params);
        }
    }*/
}

bool IrcSessionPrivate::isConnected() const
{
    return socket &&
        (socket->state() == QAbstractSocket::ConnectingState
         || socket->state() == QAbstractSocket::ConnectedState);
}

/*
QString IrcSessionPrivate::resolveTarget(const QString& sender, const QString& receiver) const
{
    QString target = receiver;

    if (target.contains(QLatin1Char('*')) || target.contains(QLatin1Char('?')))
        target = nick;

    if (target == nick)
    {
        if (target == sender)
            target = host;
        else
            target = sender;
    }

    if (target.isEmpty() || target == QLatin1String("AUTH"))
        target = host;

    return target;
}
*/

/*
IrcBuffer* IrcSessionPrivate::createBuffer(const QString& receiver)
{
    Q_Q(IrcSession);
    QString lower = receiver.toLower();
    QString lowerNick = Util::nickFromTarget(receiver).toLower();
    if (lower != lowerNick && buffers.contains(lowerNick))
    {
        IrcBuffer* buffer = buffers.value(lowerNick);
        //buffer->d_func()->setReceiver(lower);
        buffers.insert(lower, buffer);
        buffers.remove(lowerNick);
    }
    else if (!buffers.contains(lower) && !buffers.contains(lowerNick))
    {
        IrcBuffer* buffer = q->createBuffer(receiver);
        buffers.insert(lower, buffer);
        emit q->bufferAdded(buffer);
    }
    return buffers.value(lower);
}
*/

/*
void IrcSessionPrivate::removeBuffer(IrcBuffer* buffer)
{
    Q_Q(IrcSession);
    QString lower = buffer->receiver().toLower();
    if (buffers.take(lower) == buffer)
        emit q->bufferRemoved(buffer);
}
*/

bool IrcSessionPrivate::raw(const QString& msg)
{
    qint64 bytes = -1;
    if (socket)
        bytes = socket->write(msg.toUtf8() + QByteArray("\r\n"));
    return bytes != -1;
}

/*!
    Constructs a new IRC session with \a parent.
 */
IrcSession::IrcSession(QObject* parent) : QObject(parent), d_ptr(new IrcSessionPrivate(this))
{
    setSocket(new QTcpSocket(this));
}

/*!
    Destructs the IRC session.
 */
IrcSession::~IrcSession()
{
    Q_D(IrcSession);
    if (d->socket)
        d->socket->close();
}

/*!
    Returns the encoding.

    The default value is a null QByteArray.
 */
QByteArray IrcSession::encoding() const
{
    Q_D(const IrcSession);
    return d->parser.encoding();
}

/*!
    Sets the \a encoding.

    See QTextCodec documentation for supported encodings.

    Encoding auto-detection can be turned on by passing a null QByteArray.

    The fallback locale is QTextCodec::codecForLocale().
 */
void IrcSession::setEncoding(const QByteArray& encoding)
{
    Q_D(IrcSession);
    d->parser.setEncoding(encoding);
}

/*!
    Returns the host.
 */
QString IrcSession::host() const
{
    Q_D(const IrcSession);
    return d->host;
}

/*!
    Sets the \a host.
 */
void IrcSession::setHost(const QString& host)
{
    Q_D(IrcSession);
    if (d->isConnected())
        qWarning("IrcSession::setHost() has no effect until re-connect");
    d->host = host;
}

/*!
    Returns the port.
 */
quint16 IrcSession::port() const
{
    Q_D(const IrcSession);
    return d->port;
}

/*!
    Sets the \a port.
 */
void IrcSession::setPort(quint16 port)
{
    Q_D(IrcSession);
    if (d->isConnected())
        qWarning("IrcSession::setPort() has no effect until re-connect");
    d->port = port;
}

/*!
    Returns the user name.
 */
QString IrcSession::userName() const
{
    Q_D(const IrcSession);
    return d->userName;
}

/*!
    Sets the user \a name.

    \note setUserName() has no effect on already established connection.
 */
void IrcSession::setUserName(const QString& name)
{
    Q_D(IrcSession);
    if (d->isConnected())
        qWarning("IrcSession::setUserName() has no effect until re-connect");
    d->userName = name;
}

/*!
    Returns the nick name.
 */
QString IrcSession::nickName() const
{
    Q_D(const IrcSession);
    return d->nickName;
}

/*!
    Sets the nick \a name.
 */
void IrcSession::setNickName(const QString& name)
{
    Q_D(IrcSession);
    if (d->nickName != name)
    {
        d->nickName = name;
        // TODO:
        //if (d->socket)
        //    raw(QString(QLatin1String("NICK %1")).arg(nick));
    }
}

/*!
    Returns the real name.
 */
QString IrcSession::realName() const
{
    Q_D(const IrcSession);
    return d->realName;
}

/*!
    Sets the real \a name.

    \note setRealName() has no effect on already established connection.
 */
void IrcSession::setRealName(const QString& name)
{
    Q_D(IrcSession);
    if (d->isConnected())
        qWarning("IrcSession::setRealName() has no effect until re-connect");
    d->realName = name;
}

/*!
    Returns the socket.

    IrcSession creates an instance of QTcpSocket by default.

    This function was introduced in version 0.3.0.
 */
QAbstractSocket* IrcSession::socket() const
{
    Q_D(const IrcSession);
    return d->socket;
}

/*!
    Sets the \a socket. The previously set socket is deleted if its parent is \c this.

    IrcSession supports QSslSocket in the way that it automatically calls
    QSslSocket::startClientEncryption() while connecting.

    This function was introduced in version 0.3.0.
 */
void IrcSession::setSocket(QAbstractSocket* socket)
{
    Q_D(IrcSession);
    if (d->socket != socket)
    {
        if (d->socket)
        {
            d->socket->disconnect(this);
            if (d->socket->parent() == this)
                d->socket->deleteLater();
        }

        d->socket = socket;
        if (socket)
        {
            connect(socket, SIGNAL(connected()), this, SLOT(_q_connected()));
            connect(socket, SIGNAL(disconnected()), this, SLOT(_q_disconnected()));
            connect(socket, SIGNAL(readyRead()), this, SLOT(_q_readData()));
            connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(_q_error(QAbstractSocket::SocketError)));
            connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(_q_state(QAbstractSocket::SocketState)));
        }
    }
}

/*!
    Returns the main buffer.

    \sa buffer()
    \sa addBuffer()
 */
IrcBuffer* IrcSession::mainBuffer() const
{
    Q_D(const IrcSession);
    return d->mainBuffer;
}

/*!
    Returns a buffer for \a pattern. Returns \c 0
    if the buffer does not exist.

    \sa mainBuffer()
    \sa addBuffer()
 */
IrcBuffer* IrcSession::buffer(const QString& pattern) const
{
    Q_D(const IrcSession);
    return d->buffers.value(pattern);
}

/*!
    Adds a buffer for \a pattern.

    \sa removeBuffer()
 */
IrcBuffer* IrcSession::addBuffer(const QString& pattern)
{
    Q_D(IrcSession);
    IrcBuffer* buffer = createBuffer(pattern);
    d->buffers.insert(pattern, buffer);
    return buffer;
}

/*!
    Removes the \a buffer.

    \sa addBuffer()
 */
void IrcSession::removeBuffer(IrcBuffer* buffer)
{
    Q_D(IrcSession);
    if (buffer)
        d->buffers.remove(buffer->pattern(), buffer);
}

/*!
    Connects to the server.
 */
void IrcSession::open()
{
    Q_D(IrcSession);
    if (d->userName.isEmpty())
    {
        qCritical("IrcSession::open(): userName is empty!");
        return;
    }
    if (d->nickName.isEmpty())
    {
        qCritical("IrcSession::open(): nickName is empty!");
        return;
    }
    if (d->realName.isEmpty())
    {
        qCritical("IrcSession::open(): realName is empty!");
        return;
    }
    d->_q_reconnect();
}

/*!
    Disconnects from the server.
 */
void IrcSession::close()
{
    Q_D(IrcSession);
    if (d->socket)
        d->socket->disconnectFromHost();
}

/*!
    Sends a \a message to the server.
 */
bool IrcSession::sendMessage(IrcMessage* message)
{
    Q_D(IrcSession);
    return message && d->raw(message->toString());
}

/*!
    Returns a new Irc::Buffer object to act as an IRC buffer for \a pattern.

    This virtual factory method can be overridden for example in order to make
    IrcSession use a subclass of Irc::Buffer.
 */
IrcBuffer* IrcSession::createBuffer(const QString& pattern)
{
    return new IrcBuffer(pattern, this);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug debug, const IrcSession* session)
{
    if (!session)
        return debug << "IrcSession(0x0) ";
    debug.nospace() << session->metaObject()->className() << '(' << (void*) session;
    if (!session->objectName().isEmpty())
        debug << ", name = " << session->objectName();
    if (!session->host().isEmpty())
        debug << ", host = " << session->host()
              << ", port = " << session->port();
    debug << ')';
    return debug.space();
}
#endif // QT_NO_DEBUG_STREAM

#include "moc_ircsession.cpp"
