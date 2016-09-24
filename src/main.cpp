#include <QApplication>
#include <QPushButton>
#include <QResource>

#include "window.h"
#include "layer.h"




using namespace SlavGPS;




int main(int argc, char ** argv)
{
	QApplication app(argc, argv);

	QResource::registerResource("icons.rcc");
	Layer::preconfigure_interfaces();

	Window window;
	window.layers_panel->set_viewport(window.viewport); /* Ugly, FIXME. */
	window.show();

	return app.exec();
}
