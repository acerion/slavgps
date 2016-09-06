#include <QApplication>
#include <QPushButton>

#include "window_qt.h"




using namespace SlavGPS;




int main(int argc, char **argv)
{
	QApplication app (argc, argv);

	Window window;
	window.show();

	return app.exec();
}
