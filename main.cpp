#include "mainwindow.h"

int main(int argc, char *argv[])
{
int cerr = 0;
QString errStr = "", cerrStr;

    setlocale(LC_ALL,"UTF8");

    if (argc > 1) srv_port       = atoi(argv[1]); else srv_port       = 9192;  // first param - port, default 9192
    if (argc > 2) time_wait_data = atoi(argv[2]); else time_wait_data = 60000; // second param - timeout in ms, default 60000 ms

    try {
        QApplication app(argc, argv);

        MainWindow wnd(nullptr);

        wnd.show();

        app.exec();
    }

    catch (MainWindow::TheError(er)) {
        cerr = er.code;
        cerrStr.sprintf("%d", cerr);
        if (cerr > 0) {
            if (cerr & 1) errStr.append("Error create server object (" + cerrStr + ")\n");
            if (cerr & 2) errStr.append("Error starting timer_wait_data (" + cerrStr + ")\n");
        } else errStr.append("Unknown Error (" + cerrStr + ")\n");
        if (errStr.length() > 0) {
            perror((char *)cerrStr.data());
        }
        return cerr;
    }
    catch (std::bad_alloc) {
        perror("Error while alloc memory via call function new\n");
        return -1;
    }

    return 0;
}
