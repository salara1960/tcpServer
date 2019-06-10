#include "mainwindow.h"
#include "ui_mainwindow.h"

//******************************************************************************************************
//
//       TCP Server for testing stm32_sim868 project (https://github.com/salara1960/stm32_sim868)
//
//******************************************************************************************************
//char const *vers = "1.0";//06.06.2019
//char const *vers = "1.1";//07.06.2019
//char const *vers = "1.2";//09.06.2019
char const *vers = "1.3";//10.06.2019 - add wait_data_from_client timer

const QString title = "TCP server (for STM32_SIM868)";
const QString LogFileName = "logs.txt";
const int time_wait_data = 30000;//in msec.
int srv_port = 0;

const char *SeqNum = "SeqNum";
const char *DataSN = "DataSeqNum";


//******************************************************************************************************

MainWindow::MainWindow(QWidget *parent, int p) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowIcon(QIcon("png/main.png"));

    port = p;
    tcpServer = nullptr;
    MyError = 0;
    client = false;
    CliUrl.clear();
    seq_number = 0;

    fd = -1;
    pack_number = 0;
    setWindowTitle(title + " ver. " + vers);

    ui->starting->setEnabled(true);
    ui->stoping->setEnabled(false);
    ui->sending->setEnabled(false);

    tmr_sec = startTimer(1000);// 1 sec.
    if (tmr_sec <= 0) {
        MyError |= 8;//start_timer error
        throw TheError(MyError);
    }
    tmr_data = 0;
    wait_data = false;

}
//-----------------------------------------------------------------------
MainWindow::~MainWindow()
{
    killTimer(tmr_sec);
    if (tmr_data > 0) killTimer(tmr_data);
    this->disconnect();
    delete ui;
}
//-----------------------------------------------------------------------

MainWindow::TheError::TheError(int err) { code = err; }//error class descriptor

//-----------------------------------------------------------------------
void MainWindow::LogSave(const char *func, QString st, bool pr)
{
    QFile fil(LogFileName);
    if (fil.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QString fw;
        if (pr) {
            time_t ict = QDateTime::currentDateTime().toTime_t();
            struct tm *ct = localtime(&ict);
            fw.sprintf("%02d.%02d.%02d %02d:%02d:%02d | ", ct->tm_mday, ct->tm_mon+1, ct->tm_year+1900, ct->tm_hour, ct->tm_min, ct->tm_sec);
        }
        if (func) {
            fw.append("[");
            fw.append(func);
            fw.append("] ");
        }
        fw.append(st);
        ui->log->append(fw);//to log screen
        if (fw.at(fw.length() - 1) != '\n') fw.append("\n");
        fil.write(fw.toLocal8Bit(), fw.length());
        fil.close();
    }
}
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
    } else if (tmr_data == event->timerId()) {
        if (wait_data) {
            statusBar()->clearMessage();
            statusBar()->showMessage("!!! TimeOut wait data from device !!!");
            emit sigWaitDone();
        }
    }
}
//-----------------------------------------------------------------------
void MainWindow::clearParam()
{
    memset(to_cli,   0, sizeof(to_cli));
    memset(from_cli, 0, sizeof(from_cli));
    rxdata = max_buf - 3;
    txdata = 0;
    lenrecv = 0;
}
//-----------------------------------------------------------------------
void MainWindow::on_starting_clicked()
{

    clearParam();
    ui->starting->setEnabled(false);
    ui->stoping->setEnabled(true);

    tcpServer = new QTcpServer(this);

    if (!tcpServer) {
        MyError |= 1;//create server object error - no memory
        throw TheError(MyError);
    }

    QString stx = "Server start, listen port " + QString::number(port, 10);

    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(newuser()));
    if (!tcpServer->listen(QHostAddress("0.0.0.0"), port&0xffff) && !server_status) {
        stx.clear();
        stx.append("Unable to start the server : " + tcpServer->errorString());
    } else {
        server_status = 1;
        pack_number = 0;
    }

    statusBar()->clearMessage();
    statusBar()->showMessage(stx);
    LogSave(__func__, stx, true);
}
//-----------------------------------------------------------------------
void MainWindow::on_stoping_clicked()
{
    if (server_status){
        foreach(int i, SClients.keys()) {
            SClients[i]->close();
            SClients.remove(i);
        }
        tcpServer->disconnect();
        tcpServer->close();
        statusBar()->clearMessage();
        statusBar()->showMessage("Server stop.");
        LogSave(__func__, "Server stop.", true);
        server_status = 0;
        client = rdy = false;
        fd = -1;
        ui->starting->setEnabled(true);
        ui->stoping->setEnabled(false);
        ui->sending->setEnabled(false);
    }
}
//-----------------------------------------------------------------------
void MainWindow::on_sending_clicked()
{
    if (fd != -1) {
        QByteArray buf;
        buf.append(ui->ack->text());
        SClients[fd]->write(buf, buf.length());
    } else ui->sending->setEnabled(false);//block send button
}
//-----------------------------------------------------------------------
void MainWindow::newuser()
{
    if (server_status) {
        QTcpSocket *cliSocket = tcpServer->nextPendingConnection();
        fd = static_cast<int>(cliSocket->socketDescriptor());
        QString stx;
        if (!client) {
            clearParam();
            seq_number = pack_number = 0;
            client = true;
            rdy = false;

            CliUrl.clear();
            CliUrl.append(cliSocket->peerAddress().toString() + ":" + QString::number(cliSocket->peerPort(), 10));

            stx.append("New client '" + CliUrl + "' online, socket " + QString::number(fd, 10));//ssock);
            SClients[fd] = cliSocket;
            connect(SClients[fd], SIGNAL(readyRead()),                         this, SLOT(slotReadClient()));
            connect(this,         SIGNAL(sigCliDone(QTcpSocket *, int)),       this, SLOT(slotCliDone(QTcpSocket *, int)));
            connect(SClients[fd], SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(slotErrorClient(QAbstractSocket::SocketError)));
            connect(this,         SIGNAL(sigRdyPack(QTcpSocket *)),            this, SLOT(slotRdyPack(QTcpSocket *)));
            connect(this,         SIGNAL(sigWaitDone()),                       this, SLOT(slotWaitDone()));
            statusBar()->clearMessage();
            statusBar()->showMessage(stx);
            ui->sending->setEnabled(true);

            wait_data = true;
            tmr_data = startTimer(time_wait_data);//wait data from device until 30 sec
            if (tmr_data <= 0) {
                MyError |= 8;//start_timer error
                throw TheError(MyError);
            }
        } else {
            stx.append("New client '" + CliUrl + "' online, socket " + QString::number(fd, 10) + ", but client already present !");
            cliSocket->close();
        }
        LogSave(__func__, stx, true);
    }
}
//-----------------------------------------------------------------------
void MainWindow::slotWaitDone()
{
    if (tmr_data) {
        killTimer(tmr_data);
        tmr_data = 0;
    }
    wait_data = false;
    QString stx = "TimeOut wait data from device (" + QString::number(time_wait_data/1000, 10) + " sec).";
    statusBar()->clearMessage();
    statusBar()->showMessage(stx);
    LogSave(__func__, stx, true);

    if (fd != -1) emit sigCliDone(SClients[fd], 0);
}
//-----------------------------------------------------------------------
void MainWindow::slotReadClient()
{
QTcpSocket *cliSocket = dynamic_cast<QTcpSocket *>(sender());

    lenrecv += cliSocket->read(from_cli + lenrecv, rxdata - lenrecv);

    if ( (strstr(from_cli, "exit"))  ||
            (strstr(from_cli, "}\r\n")) ||
                (lenrecv >= max_buf - 2) ) rdy = true;

    if (rdy && lenrecv) emit sigRdyPack(cliSocket);
}
//-----------------------------------------------------------------------
void MainWindow::slotCliDone(QTcpSocket *cli, int prn)
{
    if (cli->isOpen()) {
        if (prn) {
            QString stx = "Close client, socket " + QString::number(fd, 10);
            LogSave(__func__, stx, true);
            statusBar()->clearMessage();
            statusBar()->showMessage(stx);
            if (strlen(from_cli)) {
                QString dt; dt.append(from_cli);
                LogSave(__func__, dt, 1);
            }
        }
        disconnect(cli);
        cli->close();
        ui->sending->setEnabled(false);

    }
    this->disconnect();
    SClients.remove(fd);
    fd = -1;
    client = rdy = false;

}
//-----------------------------------------------------------------------
void MainWindow::slotErrorClient(QAbstractSocket::SocketError SErr)
{
QTcpSocket *cliSocket = reinterpret_cast<QTcpSocket *>(sender());
QString qs = "Socket ERROR (" + QString::number(static_cast<int>(SErr), 10) + ") : " + cliSocket->errorString();

    QString stx = "Close client, socket " + QString::number(cliSocket->socketDescriptor(), 10) + ". " + qs;
    LogSave(__func__, stx, true);
    statusBar()->clearMessage();
    statusBar()->showMessage(stx);

    emit sigCliDone(cliSocket, 0);
}
//-----------------------------------------------------------------------
void MainWindow::slotRdyPack(QTcpSocket *cli)
{

    if (!lenrecv) return;

    QString dt;
    LogSave(__func__, dt.append(from_cli), 1);
    char numName[64] = {0};
    int seqn = CheckPack(from_cli, numName);
    int len = sprintf(to_cli, "{\"PackNumber\":%u,\"%s\":%d}\n", ++pack_number, numName, seqn);
    cli->write(to_cli, len);
    dt.clear();
    ui->ack->setText(dt.append(to_cli));
    LogSave(__func__, dt, 1);


    clearParam();
    rdy = false;

    wait_data = true;
    tmr_data = startTimer(time_wait_data);//wait data from device until 30 sec
    if (tmr_data <= 0) {
        MyError |= 8;//start_timer error
        throw TheError(MyError);
    }

}
//-----------------------------------------------------------------------
int MainWindow::CheckPack(const char *in, char *packName)
{
int ret = -1;

    if (strstr(in, SeqNum) == nullptr) {
        strcpy(packName, "Unknown");
        return ret;
    }

    QJsonParseError err;
    QByteArray buf(in);
    QString stx;
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
                stx = "Data parser OK";
            } else {
                strcpy(packName, "Unknown");
                stx = "Data parser error : unknown";
            }
        }
    }

    LogSave(__func__, stx, 1);
    statusBar()->clearMessage();
    statusBar()->showMessage(stx);

    if (obj) delete obj; 

    return ret;
}





