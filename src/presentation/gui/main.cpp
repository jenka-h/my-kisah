#include "presentation/gui/MainWindow.h"

#include <QApplication>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("My Kisah Decompiler"));
    application.setOrganizationName(QStringLiteral("SISTER2"));

    mykisah::gui::MainWindow window;
    window.show();

    if (argc == 2) {
        window.open_binary(QString::fromLocal8Bit(argv[1]));
    } else if (argc == 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--smoke-test")) {
        window.open_binary(QString::fromLocal8Bit(argv[2]));
        QTimer::singleShot(0, &application, &QApplication::quit);
    }

    return application.exec();
}
