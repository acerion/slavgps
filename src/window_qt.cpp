#include <QtWidgets>

#include "window_qt.h"
#include "vikviewport.h"
#include "viklayer.h"
#include "viklayerspanel.h"


Window::Window()
{
	this->create_layout();
	this->create_actions();
}




void Window::create_layout()
{
	//QHBoxLayout * layout = new QHBoxLayout;
	QWidget * central_widget = new QWidget;
	//central_widget->setLayout(layout);


	this->layers_panel = new SlavGPS::LayersPanel(central_widget);


	QDockWidget * dock = new QDockWidget(this);
	dock->setWidget(this->layers_panel);
	dock->setWindowTitle("Layers");
	this->addDockWidget(Qt::LeftDockWidgetArea, dock);

	setStyleSheet("QMainWindow::separator { image: url(src/icons/handle_indicator.png); width: 8}");



	SlavGPS::Viewport * viewport = new SlavGPS::Viewport(this);
	viewport->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	//layout->addWidget(viewport);
	struct LatLon ll = { 22.0, 27.0 };
	viewport->set_center_latlon(&ll, false);
	viewport->xmpp = 0.01;
	viewport->ympp = 0.01;
	//viewport->show();
	fprintf(stderr, "%d / %d\n", viewport->height(), viewport->width());


	SlavGPS::Layer * layer = SlavGPS::Layer::new_(SlavGPS::LayerType::COORD, viewport, false);
	viewport->configure();
	layer->draw(viewport);

	this->setCentralWidget(viewport);

	return;
}




void Window::create_actions(void)
{
	QMenu * menu_file = new QMenu("File");
	QMenu * menu_edit = new QMenu("Edit");
	QMenu * menu_view = new QMenu("View");
	QMenu * menu_layers = new QMenu("Layers");
	QMenu * menu_tools = new QMenu("Tools");
	QMenu * menu_help = new QMenu("Help");

	this->menu_bar = new QMenuBar();
	this->menu_bar->addMenu(menu_file);
	this->menu_bar->addMenu(menu_edit);
	this->menu_bar->addMenu(menu_view);
	this->menu_bar->addMenu(menu_layers);
	this->menu_bar->addMenu(menu_tools);
	this->menu_bar->addMenu(menu_help);
	setMenuBar(this->menu_bar);

	QAction * qa_file_new = new QAction("New file...", this);
	qa_file_new->setIcon(QIcon::fromTheme("document-new"));

	QAction * qa_layer_properties = new QAction("Properties...", this);

	QAction * qa_help_help = new QAction("Help", this);
	qa_help_help->setIcon(QIcon::fromTheme("help-contents"));

	QAction * qa_help_about = new QAction("About", this);
	qa_help_about->setIcon(QIcon::fromTheme("help-about"));


	menu_file->addAction(qa_file_new);

	menu_layers->addAction(qa_layer_properties);

	menu_help->addAction(qa_help_help);
	menu_help->addAction(qa_help_about);

	this->tool_bar = new QToolBar();
	addToolBar(this->tool_bar);

	this->tool_bar->addAction(qa_file_new);




	this->status_bar = new QStatusBar();
	setStatusBar(this->status_bar);

	return;
}
