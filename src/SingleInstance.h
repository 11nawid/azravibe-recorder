// Single-instance guard for Azravibe Recorder.
// First launch: creates QLocalServer "azravibe-recorder-<user>" and serves "raise".
// Later launches: connect, send "raise", and exit — instead of opening a second window.
#pragma once

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>

#include <functional>
#include <QString>

class SingleInstanceGuard {
public:
    SingleInstanceGuard() {
        const QString user = QString::fromLocal8Bit(qgetenv("USER"));
        name = QStringLiteral("azravibe-recorder-%1").arg(user.isEmpty() ? QStringLiteral("default") : user);

        socket.connectToServer(name);
        secondary = socket.waitForConnected(300);

        if (secondary) {
            socket.write("raise\n");
            socket.waitForBytesWritten(300);
            socket.disconnectFromServer();
            return;
        }

        QLocalServer::removeServer(name);       // clean stale listener from a crashed session
        server.listen(name);

        QObject::connect(&server, &QLocalServer::newConnection, [this] {
            QLocalSocket* c = server.nextPendingConnection();
            if (!c) return;
            QObject::connect(c, &QLocalSocket::readyRead, c, [this, c] {
                const QByteArray line = c->readAll().trimmed();
                c->deleteLater();
                if (line == "raise" && onRaise) onRaise();
            });
        });
    }

    bool isSecondaryInstance() const { return secondary; }

    std::function<void()> onRaise;   // set before show() to focus the existing window

private:
    QLocalSocket socket;
    QLocalServer server;
    QString      name;
    bool         secondary = false;
};
