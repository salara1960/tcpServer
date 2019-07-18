#include "mainwindow.h"
#include "ui_mainwindow.h"

//********************************************************************************************************
//
//  TCP Server (thread mode) for testing stm32_sim868 project (https://github.com/salara1960/stm32_sim868)
//
//********************************************************************************************************

//char const *vers = "2.0";//14.06.2019 - add thread for each client
//char const *vers = "2.1";//17.06.2019 - add endofjob all client's threads by signal sigStopAll
//char const *vers = "2.2";//17.06.2019 - add queue for print log via LogSave(...)
//char const *vers = "2.3";//18.06.2019 - minor changes in print_queue : edit s_prn (used dynamic memory)
//char const *vers = "2.4";//18.06.2019 - minor changes : add inactive client's timer
//char const *vers = "2.5";//18.06.2019 - minor changes+
//char const *vers = "2.6";//18.07.2019 - minor changes : gui form change - add port item
char const *vers = "2.7";//18.07.2019 - minor changes++

const QString title = "TCP server (for STM32_SIM868)";

int srv_port = 9192;

const QString LogFileName = "logs.txt";

const char *SeqNum = "SeqNum";
const char *DataSN = "InfSeqNum";
MainWindow *Uki = nullptr;

static uint32_t total_cli = 0;
int tmr_data_wait = 30000;


//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

//-----------------------------------------------------------------------
itThread::itThread(QTcpSocket *soc)
{
    socket = soc;
    client = new itClient(socket);

    //client->moveToThread(this);

    fd = client->getFD();

    connect(socket, SIGNAL(disconnected()), this, SLOT(stop()));

    tid = nullptr;
    fls = 0;
}
//-----------------------------------------------------------------------
void itThread::run()
{
    tid = reinterpret_cast<void *>(this->currentThread());

    QString st; st.sprintf("Start new thread (%p). Total clients %u.", tid, total_cli);

    Uki->putToQue(__func__, st, true, fd);

    exec();
}
//-----------------------------------------------------------------------
void itThread::stop()
{
    if (fls) return;
    fls = 1;

    if (total_cli) total_cli--;

    this->disconnect();

    QString st; st.sprintf("Stop thread (%p). Total clients %u.", tid, total_cli);

    Uki->putToQue(__func__, st, true, fd);

    socket->close();
    socket->disconnect();

    delete client;

    exit(0);
}
//-----------------------------------------------------------------------

//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

itClient::itClient(QTcpSocket *soc)
{
    socket = soc;
    fd = static_cast<int>(socket->socketDescriptor());
    err = 0;
    tid = this->thread();
    clearParam();
    pack_number = 0;

    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(slotErrorClient(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(readyRead()),    this, SLOT(slotReadClient()), Qt::DirectConnection);
    //connect(socket, SIGNAL(disconnected()), this, SLOT(stop()));

    connect(this,   SIGNAL(sigRdyPack()),   this, SLOT(slotRdyPack()));
    connect(this,   SIGNAL(sigDone()),      this, SLOT(stop()));
    connect(this,   SIGNAL(sigTime()),      this, SLOT(slotTime()));

    tmr_data.singleShot(tmr_data_wait, this, SLOT(slotTime()));
    epoch = QDateTime::currentDateTime().toTime_t();

}
//-----------------------------------------------------------------------
int itClient::getFD()
{   
    return fd;
}
//-----------------------------------------------------------------------
QTcpSocket *itClient::getSoc()
{
    return socket;
}
//-----------------------------------------------------------------------
void itClient::clearParam()
{
    memset(to_cli,   0, sizeof(to_cli));
    memset(from_cli, 0, sizeof(from_cli));
    rxdata = max_buf - 3;
    txdata = 0;
    lenrecv = 0;
    rdy = false;
}
//-----------------------------------------------------------------------
void itClient::slotTime()
{
    time_t now = QDateTime::currentDateTime().toTime_t();
    long delta = reinterpret_cast<long>(now) - reinterpret_cast<long>(epoch);
    long isec = tmr_data_wait / 1000;

    Uki->toStatusBar("Timeout " + QString::number(isec, 10) + " sec. Done thread socket " + QString::number(fd, 10) + ". ");

    if (delta >= isec) emit sigDone();
                  else tmr_data.singleShot(tmr_data_wait, this, SLOT(slotTime()));

}
//-----------------------------------------------------------------------
void itClient::slotReadClient()
{
    lenrecv += socket->read(from_cli + lenrecv, rxdata - lenrecv);

    if ( (strstr(from_cli, "exit"))  || (strstr(from_cli, "}\r\n")) || (lenrecv >= max_buf - 2) ) rdy = true;

    if (rdy) emit sigRdyPack();
}
//-----------------------------------------------------------------------
void itClient::slotRdyPack()
{
    if (!lenrecv) return;

    epoch = QDateTime::currentDateTime().toTime_t();

    QString dt; dt.sprintf("tid=%p\n", tid);
    int seqn = -1;
    char numName[64] = {0};
    char *uk = strstr(from_cli, "}\r\n");
    if (uk) *(uk + 1) = '\0';

    Uki->putToQue(__func__, dt.append(from_cli), true, fd);

    seqn = Uki->CheckPack(from_cli, numName);
    int len = sprintf(to_cli, "{\"PackNumber\":%u,\"%s\":%d}\n", ++pack_number, numName, seqn);
    socket->write(to_cli, len);

    dt.clear();
    dt.append(to_cli);
    Uki->setAck(dt);

    if (strstr(from_cli, "exit")) emit sigDone();

    clearParam();

    tmr_data.singleShot(tmr_data_wait, this, SLOT(slotTime()));

}
//-----------------------------------------------------------------------
void itClient::slotErrorClient(QAbstractSocket::SocketError SErr)
{
QString qs = "Socket ERROR (" + QString::number(static_cast<int>(SErr), 10) + ") : " + socket->errorString();

    QString stx = "Close client, socket " + QString::number(fd, 10) + ". " + qs;

    Uki->putToQue(__func__, stx, true, fd);
    Uki->toStatusBar(stx);

    err = 1;

    emit sigDone();
}
//-----------------------------------------------------------------------
void itClient::stop()
{
    if (total_cli) total_cli--;

    disconnect(this, SIGNAL(sigDone()));
/*
    if (!err) {
        if (socket->isOpen()) socket->close();
    }
*/
}

//*********************************************************************************************
//*********************************************************************************************
//*********************************************************************************************

void MainWindow::setBut(bool en)
{
    ui->sending->setEnabled(en);
}
//-------------------------------------------------------------------------------------------
void MainWindow::setAck(QString &qstr)
{
    ui->ack->setText(qstr);
}
//-------------------------------------------------------------------------------------------
void MainWindow::toStatusBar(const QString &qstr)
{
    statusBar()->clearMessage();
    statusBar()->showMessage(qstr);
}
//-------------------------------------------------------------------------------------------
int MainWindow::CheckPack(const char *in, char *packName)
{
int ret = -1;

    if (strstr(in, SeqNum) == nullptr) {
        strcpy(packName, "Unknown");
        return ret;
    }

    QJsonParseError err;
    QByteArray buf(in);
    QString stx; stx.clear();
    QJsonObject *obj = nullptr;

    QJsonDocument jdoc= QJsonDocument::fromJson(buf , &err);
    if (err.error != QJsonParseError::NoError) {
        stx = "Data parser error : " + err.errorString();
    } else {
        obj = new QJsonObject(jdoc.object());
        if (!obj) {
            stx = "Data parser error : " + err.errorString();
        } else {
            QJsonValue tmp = obj->value(DataSN);
            if (!tmp.isUndefined()) {
                ret = tmp.toInt();
                strcpy(packName, DataSN);
                //stx = "Data parser OK";
            } else {
                strcpy(packName, "Unknown");
                stx = "Data parser error : unknown";
            }
        }
    }

    if (stx.length() > 0) putToQue(__func__, stx, true, -1);

    statusBar()->clearMessage();
    statusBar()->showMessage(stx);

    if (obj) delete obj;

    return ret;
}
//-------------------------------------------------------------------------------------------
void MainWindow::LogSave(const char *func, QString st, bool pr, int from)
{
    QFile fil(LogFileName);
    if (fil.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (mutex_prn.tryLock(1000)) {
            //
            QString fw;
            if (pr) {
                time_t ict = QDateTime::currentDateTime().toTime_t();
                struct tm *ct = localtime(&ict);
                fw.sprintf("%02d.%02d.%02d %02d:%02d:%02d | ",
                           ct->tm_mday, ct->tm_mon+1, ct->tm_year+1900,
                           ct->tm_hour, ct->tm_min, ct->tm_sec);
            }
            if (func) {
                fw.append("[");
                fw.append(func);
                fw.append(" : " + QString::number(from, 10));
                fw.append("] ");
            }
            fw.append(st);
            ui->log->append(fw);
            if (fw.at(fw.length() - 1) != '\n') fw.append("\n");
            fil.write(fw.toLocal8Bit(), fw.length());
            //
            mutex_prn.unlock();
        }

        fil.close();
    }
}
//-------------------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Uki = this;
    this->setWindowIcon(QIcon("png/main.png"));

    tcpServer = nullptr;
    MyError = 0;
    total_cli = 0;

    setWindowTitle(title + " ver. " + vers);

    ui->starting->setEnabled(true);
    ui->stoping->setEnabled(false);
    ui->sending->setEnabled(false);

    tmr_sec = startTimer(1000);// 1 sec.
    if (tmr_sec <= 0) {
        MyError |= 2;
        throw TheError(MyError);
    }

    ui->port->setText(QString::number(srv_port, 10));

    connect(this, SIGNAL(sigNewPrn()), this, SLOT(getFromQue()));

}
//-----------------------------------------------------------------------
MainWindow::~MainWindow()
{
    while (!que.isEmpty()) {
        s_prn t = que.dequeue();
        delete t.fn;
        delete t.st;
    }
    killTimer(tmr_sec);
    this->disconnect();
    delete ui;
}
//-----------------------------------------------------------------------

MainWindow::TheError::TheError(int err) { code = err; }

//-----------------------------------------------------------------------
void MainWindow::timerEvent(QTimerEvent *event)
{ 
    if (tmr_sec == event->timerId()) {
        time_t it_ct = QDateTime::currentDateTime().toTime_t();
        struct tm *ctimka = localtime(&it_ct);
        QString dt;
        dt.sprintf(" | %02d.%02d.%02d %02d:%02d:%02d",
                    ctimka->tm_mday,
                    ctimka->tm_mon+1,
                    ctimka->tm_year+1900,
                    ctimka->tm_hour,
                    ctimka->tm_min,
                    ctimka->tm_sec);
        setWindowTitle(title + " ver. " + vers + dt);
    }
}
//-----------------------------------------------------------------------
void MainWindow::putToQue(const char *fn, QString st, bool pr, int from)
{
    s_prn two;
    two.fd = from;
    two.pr = pr;
    two.st = new QString(st);
    two.fn = new QByteArray(fn);
    que.enqueue(two);
    emit sigNewPrn();
}
//-----------------------------------------------------------------------
void MainWindow::getFromQue()
{
    if (!que.isEmpty()) {
        s_prn t = que.dequeue();
        QByteArray ar(*t.fn);
        LogSave(ar.data(), *t.st, t.pr, t.fd);
        delete t.fn;
        delete t.st;
    }
}
//-----------------------------------------------------------------------
void MainWindow::on_starting_clicked()
{

    ui->stoping->setEnabled(true);

    tcpServer = new QTcpServer(this);

    if (!tcpServer) {
        MyError |= 1;
        throw TheError(MyError);
    }

    bool oks;
    int prt = ui->port->text().toInt(&oks, 10);
    if (oks) srv_port = prt;
    QString port_str = QString::number(srv_port, 10);

    QString stx =   "Server start, listen port " + port_str +
                    ", timeout " +                 QString::number(tmr_data_wait/1000, 10) +
                    " sec.";

    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(newuser()));


    if (!tcpServer->listen(QHostAddress("0.0.0.0"), srv_port & 0xffff) && !server_status) {
        stx.clear();
        stx.append("Unable to start server on port " + port_str + " : " + tcpServer->errorString());
        ui->starting->setEnabled(true);
    } else {
        server_status = 1;
        ui->starting->setEnabled(false);
    }

    statusBar()->clearMessage();
    statusBar()->showMessage(stx);

    putToQue(__func__, stx, true, -1);
}
//-----------------------------------------------------------------------
void MainWindow::on_stoping_clicked()
{
    if (server_status) {

        if (total_cli) emit sigStopAll();

        tcpServer->disconnect();
        tcpServer->close();
        QString stx = "Server stop.";
        statusBar()->clearMessage();
        statusBar()->showMessage(stx);

        putToQue(__func__, stx, true, -1);

        server_status = 0;

        ui->starting->setEnabled(true);
        ui->stoping->setEnabled(false);
        ui->sending->setEnabled(false);
    }
}
//-----------------------------------------------------------------------
void MainWindow::newuser()
{
    if (server_status) {
        QTcpSocket *soc = tcpServer->nextPendingConnection();
        int fd = static_cast<int>(soc->socketDescriptor());      
        QString stx = "New client '" + soc->peerAddress().toString() +
                ":" + QString::number(soc->peerPort(), 10) +
                "' online, socket " + QString::number(fd, 10);
        statusBar()->clearMessage();
        statusBar()->showMessage(stx);
        //ui->sending->setEnabled(true);

        putToQue(__func__, stx, true, fd);

        ui->client->setText(soc->peerAddress().toString() + ":" + QString::number(soc->peerPort(), 10));

        //itClient *cli = new itClient(soc);

        itThread *th = new itThread(soc);
        if (th) {
            total_cli++;
            //cli->moveToThread(th);
            connect(th,   SIGNAL(finished()),   th, SLOT(deleteLater()));
            connect(this, SIGNAL(sigStopAll()), th, SLOT(stop()));
            th->start();
        }

    }
}
//-----------------------------------------------------------------------







