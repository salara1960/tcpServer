#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <inttypes.h>
#include <iostream>
#include <arpa/inet.h>
#include <time.h>
#include <endian.h>
#include <stdlib.h>
#include <string.h>

#include <QApplication>
#include <QMainWindow>
#include <QtNetwork>
#include <QTcpSocket>
#include <QTcpServer>
#include <QtNetwork/QAbstractSocket>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>


//********************************************************************************

#define SET_DEBUG

#define max_buf 2048

//********************************************************************************

extern const char *vers;

extern const QString title;
extern const QString LogFileName;
extern int time_wait_data;
extern int srv_port;

extern const char *SeqNum;
extern const char *DataSN;

//********************************************************************************

namespace Ui {
class MainWindow;
}
//********************************************************************************
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    class TheError {
        public :
            int code;
            TheError(int);
    };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void timerEvent(QTimerEvent *event);

public slots:
    void LogSave(const char *func, QString st, bool pr);
    void clearParam();
    int CheckPack(const char *, char *);
    void slotErrorClient(QAbstractSocket::SocketError);

private slots:
    void on_starting_clicked();
    void on_stoping_clicked();
    void on_sending_clicked();
    void newuser();
    void slotReadClient();
    void slotCliDone(QTcpSocket *, int);
    void slotRdyPack(QTcpSocket *);
    void slotWaitDone();

signals:
    void sigWaitDone();
    void sigCliDone(QTcpSocket *, int);
    void sigRdyPack(QTcpSocket *);
    void sigSending();

private:
    Ui::MainWindow *ui;

    bool client, rdy, work;
    int MyError, rxdata, txdata, lenrecv;
    int fd, tmr_sec, server_status;
    char to_cli[max_buf], from_cli[max_buf];
    qint32 seq_number, pack_number;
    QString CliUrl;
    QTcpServer *tcpServer;
    QMap <int, QTcpSocket *> SClients;

    bool wait_data;
    int tmr_data;
};

#endif // MAINWINDOW_H
