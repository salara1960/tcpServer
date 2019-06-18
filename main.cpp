#include "mainwindow.h"

int main(int argc, char *argv[])
{
int cerr = 0;
QString errStr = "", cerrStr;

    setlocale(LC_ALL,"UTF8");

    if (argc > 1) srv_port      = atoi(argv[1]);
    if (argc > 2) tmr_data_wait = atoi(argv[2]);

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
