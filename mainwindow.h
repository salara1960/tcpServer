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
#include <QThread>
#include <QLineEdit>
#include <QDebug>

#include <QString>
#include <QMutex>
#include <QQueue>

//********************************************************************************

#define max_buf 2048

//********************************************************************************

typedef struct {
    int fd;
    bool pr;
    QString *st;
    QByteArray *fn;
} s_prn;

//********************************************************************************

class MainWindow;

//********************************************************************************
extern const QString LogFileName;
extern int tmr_data_wait;
extern int srv_port;

extern const char *SeqNum;
extern const char *DataSN;
extern MainWindow *Uki;

extern const char *vers;
extern const QString title;

//********************************************************************************
class itThread : public QThread
{
  Q_OBJECT

  public:
    explicit itThread(QTcpSocket *soc, QObject *parent = nullptr);
    void run();

signals:
    void sigRdyPack();
    void sigTime();
    void sigDone();
    void error(QTcpSocket::SocketError);

public slots:
    void clearParam();
    void slotReadClient();
    void slotRdyPack();
    void slotErrorClient(QAbstractSocket::SocketError);
    void slotTime();
    void stop();

    friend class MainWindow;

private:
    QTcpSocket *socket;
    bool rdy;
    int rxdata, txdata, lenrecv, fd, err;
    char to_cli[max_buf], from_cli[max_buf];
    qint32 seq_number, pack_number;
    QTimer tmr_data;
    time_t epoch;
    void *tid;
};


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
    void LogSave(const char *func, QString st, bool pr, int from);
    int CheckPack(const char *in, char *packName);
    void setBut(bool en);
    void setAck(QString &);
    void toStatusBar(const QString &);
    //
    void putToQue(const char *, QString, bool, int);
    void getFromQue();

private slots:
    void on_starting_clicked();
    void on_stoping_clicked();
    void newuser();

signals:
    void sigStopAll();
    //
    void sigNewPrn();

private:
    Ui::MainWindow *ui;

    int MyError, tmr_sec, server_status;
    QTcpServer *tcpServer;
    QMutex mutex_prn;
    //
    QQueue <s_prn> que;
};

#endif // MAINWINDOW_H
