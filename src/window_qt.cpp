#include <QtWidgets>

#include "window_qt.h"
#include "vikviewport.h"
#include "viklayer.h"
#include "viklayerspanel.h"
#include "globals.h"
#include "uibuilder_qt.h"




using namespace SlavGPS;




static LayerParamScale param_scales[] = {
	{ 0.05, 60.0, 0.25, 10 },
	{ 1, 10, 1, 0 },
};


static LayerParamData color_default(void)
{
	LayerParamData data;
	data.c.r = 1;
	data.c.g = 1;
	data.c.b = 1;
	data.c.a = 1;
	return data;
	// or: return VIK_LPD_COLOR (0, 65535, 0, 0);
}

static LayerParamData min_inc_default(void)
{
	return VIK_LPD_DOUBLE (1.0);
}
static LayerParamData line_thickness_default(void)
{
	return VIK_LPD_UINT (3);
}



Window::Window()
{

	QIcon::setThemeName("Tango");
	this->create_layout();
	this->create_actions();


	// Own signals
	connect(this->viewport, SIGNAL(updated_center(void)), this, SLOT(center_changed_cb(void)));


	//this->layers_panel->new_layer(SlavGPS::LayerType::COORD);





	static LayerParam layer_params[] = {
		/* Layer type       name              param type                group                   title              widget type                         widget data       extra widget data       tooltip        default value              convert to display        convert to internal */
		{ LayerType::COORD, "color",          LayerParamType::STRING,   VIK_LAYER_GROUP_NONE,   "Entry:",          LayerWidgetType::ENTRY,             NULL,             NULL,                   NULL,          NULL,                      NULL,                     NULL },
		{ LayerType::COORD, "color",          LayerParamType::BOOLEAN,  VIK_LAYER_GROUP_NONE,   "Checkbox:",       LayerWidgetType::CHECKBUTTON,       NULL,             NULL,                   NULL,          NULL,                      NULL,                     NULL },

		{ LayerType::COORD, "color",          LayerParamType::COLOR,    VIK_LAYER_GROUP_NONE,   "Color:",          LayerWidgetType::COLOR,             NULL,             NULL,                   NULL,          color_default,             NULL,                     NULL },
		{ LayerType::COORD, "min_inc",        LayerParamType::DOUBLE,   VIK_LAYER_GROUP_NONE,   "Minutes Width:",  LayerWidgetType::SPINBOX_DOUBLE,    &param_scales[0], NULL,                   NULL,          min_inc_default,           NULL,                     NULL },
		{ LayerType::COORD, "line_thickness", LayerParamType::UINT,     VIK_LAYER_GROUP_NONE,   "Line Thickness:", LayerWidgetType::SPINBUTTON,        &param_scales[1], NULL,                   NULL,          line_thickness_default,    NULL,                     NULL },
	};



	LayerPropertiesDialog dialog(this);
	dialog.fill(layer_params, 5);
	dialog.exec();
}




void Window::create_layout()
{
	//QHBoxLayout * layout = new QHBoxLayout;
	QWidget * central_widget = new QWidget;
	//central_widget->setLayout(layout);


	this->layers_panel = new SlavGPS::LayersPanel(this);


	QDockWidget * dock = new QDockWidget(this);
	dock->setWidget(this->layers_panel);
	dock->setWindowTitle("Layers");
	this->addDockWidget(Qt::LeftDockWidgetArea, dock);

	setStyleSheet("QMainWindow::separator { image: url(src/icons/handle_indicator.png); width: 8}");



	this->viewport = new SlavGPS::Viewport(this);
	this->viewport->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	//layout->addWidget(viewport);
	struct LatLon ll = { 22.0, 27.0 };
	this->viewport->set_center_latlon(&ll, false);
	this->viewport->xmpp = 0.01;
	this->viewport->ympp = 0.01;
	//viewport->show();
	fprintf(stderr, "%d / %d\n", this->viewport->height(), this->viewport->width());

	this->layers_panel->set_viewport(this->viewport);


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



	QAction * qa_help_help = new QAction("Help", this);
	qa_help_help->setIcon(QIcon::fromTheme("help-contents"));

	QAction * qa_help_about = new QAction("About", this);
	qa_help_about->setIcon(QIcon::fromTheme("help-about"));


	menu_file->addAction(qa_file_new);

	{
		this->qa_layer_properties = new QAction("Properties...", this);
		menu_layers->addAction(this->qa_layer_properties);
		connect (this->qa_layer_properties, SIGNAL (triggered(bool)), this->layers_panel, SLOT (properties(void)));

		for (SlavGPS::LayerType i = SlavGPS::LayerType::AGGREGATE; i < SlavGPS::LayerType::NUM_TYPES; ++i) {

			QVariant variant((int) i);
			QAction * qa = new QAction("new layer", this);
			qa->setData(variant);
			connect(qa, SIGNAL(triggered(bool)), this, SLOT(menu_layer_new_cb(void)));

			menu_layers->addAction(qa);
		}
	}

	menu_help->addAction(qa_help_help);
	menu_help->addAction(qa_help_about);

	this->tool_bar = new QToolBar();
	addToolBar(this->tool_bar);

	this->tool_bar->addAction(qa_file_new);




	this->status_bar = new QStatusBar();
	setStatusBar(this->status_bar);

	return;
}




void Window::draw_update()
{
	this->draw_redraw();
	this->draw_sync();
}




static void draw_sync_cb(Window * window)
{
	window->draw_sync();
}




void Window::draw_sync()
{
	this->viewport->sync();
	this->draw_status();
}




void Window::draw_status()
{
	static char zoom_level[22];
	double xmpp = this->viewport->get_xmpp();
	double ympp = this->viewport->get_ympp();
	char *unit = this->viewport->get_coord_mode() == VIK_COORD_UTM ? (char *) "mpp" : (char *) "pixelfact";
	if (xmpp != ympp) {
		snprintf(zoom_level, 22, "%.3f/%.3f %s", xmpp, ympp, unit);
	} else {
		if ((int)xmpp - xmpp < 0.0) {
			snprintf(zoom_level, 22, "%.3f %s", xmpp, unit);
		} else {
			/* xmpp should be a whole number so don't show useless .000 bit. */
			snprintf(zoom_level, 22, "%d %s", (int)xmpp, unit);
		}
	}
#if 0
	vik_statusbar_set_message(this->viking_vs, VIK_STATUSBAR_ZOOM, zoom_level);

	draw_status_tool(this);
#endif
}




void Window::menu_layer_new_cb(void) /* Slot. */
{
	QAction * qa = (QAction *) QObject::sender();
	SlavGPS::LayerType layer_type = (SlavGPS::LayerType) qa->data().toInt();

	fprintf(stderr, "clicked layer new for layer type %d\n", (int) layer_type);

	if (this->layers_panel->new_layer(layer_type)) {
		this->draw_update();
		this->modified = true;
	}

}




void Window::draw_redraw()
{
	VikCoord old_center = this->trigger_center;
	this->trigger_center = *(this->viewport->get_center());
	SlavGPS::Layer * new_trigger = this->trigger;
	this->trigger = NULL;
	SlavGPS::Layer * old_trigger = this->viewport->get_trigger();

	if (!new_trigger) {
		; /* Do nothing -- have to redraw everything. */
	} else if ((old_trigger != new_trigger) || !vik_coord_equals(&old_center, &this->trigger_center) || (new_trigger->type == SlavGPS::LayerType::AGGREGATE)) {
		this->viewport->set_trigger(new_trigger); /* todo: set to half_drawn mode if new trigger is above old */
	} else {
		this->viewport->set_half_drawn(true);
	}

	/* Actually draw. */
	this->viewport->clear();
	/* Main layer drawing. */
	this->layers_panel->draw_all();
	/* Draw highlight (possibly again but ensures it is on top - especially for when tracks overlap). */
#if 0
	if (this->viewport->get_draw_highlight()) {
		if (this->containing_trw && (this->selected_tracks || this->selected_waypoints)) {
			this->containing_trw->draw_highlight_items(this->selected_tracks, this->selected_waypoints, this->viewport);

		} else if (this->containing_trw && (this->selected_track || this->selected_waypoint)) {
			this->containing_trw->draw_highlight_item((Track *) this->selected_track, this->selected_waypoint, this->viewport);

		} else if (this->selected_trw) {
			this->selected_trw->draw_highlight(this->viewport);
		}
	}
#endif
	/* Other viewport decoration items on top if they are enabled/in use. */
	this->viewport->draw_scale();
	this->viewport->draw_copyright();
	this->viewport->draw_centermark();
	this->viewport->draw_logo();

	this->viewport->set_half_drawn(false); /* Just in case. */
}




void Window::selected_layer(Layer * layer)
{
#if 0
	if (!this->action_group) {
		return;
	}

	for (LayerType type = LayerType::AGGREGATE; type < LayerType::NUM_TYPES; ++type) {
		VikLayerInterface * layer_interface = vik_layer_get_interface(type);
		int tool_count = layer_interface->tools_count;

		for (int tool = 0; tool < tool_count; tool++) {
			GtkAction * action = gtk_action_group_get_action(this->action_group,
									 layer_interface->layer_tools[tool]->radioActionEntry.name);
			g_object_set(action, "sensitive", type == layer->type, NULL);
			toolbar_action_set_sensitive(this->viking_vtb, vik_layer_get_interface(type)->layer_tools[tool]->radioActionEntry.name, type == layer->type);
		}
	}
#endif
}




Viewport * Window::get_viewport()
{
	return this->viewport;
}



LayersPanel * Window::get_layers_panel()
{
	return this->layers_panel;
}




/**
 * center_changed_cb:
 */
void Window::center_changed_cb(void) /* Slot. */
{
	fprintf(stderr, "---- handling updated_center signal (%s:%d)\n", __FUNCTION__, __LINE__);
#if 0
	// ATM Keep back always available, so when we pan - we can jump to the last requested position
	/*
	  GtkAction* action_back = gtk_action_group_get_action(window->action_group, "GoBack");
	  if (action_back) {
	  gtk_action_set_sensitive(action_back, vik_viewport_back_available(window->viewport));
	  }
	*/
	GtkAction* action_forward = gtk_action_group_get_action(this->action_group, "GoForward");
	if (action_forward) {
		gtk_action_set_sensitive(action_forward, this->viewport->forward_available());
	}

	toolbar_action_set_sensitive(this->viking_vtb, "GoForward", this->viewport->forward_available());
#endif
}




QMenu * Window::get_layer_menu(QMenu * menu)
{
	menu->addAction(this->qa_layer_properties);
	return menu;
}
