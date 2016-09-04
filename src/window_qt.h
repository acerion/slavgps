#ifndef _SG_WINDOW_QT_H_
#define _SG_WINDOW_QT_H_




#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTreeView>

#include "viklayerspanel.h"
#include "vikviewport.h"


class Window : public QMainWindow {
	Q_OBJECT

public:
	Window();


protected:

private slots:

private:

	void create_layout(void);
	void create_actions();

	QMenuBar * menu_bar = NULL;
	QToolBar * tool_bar = NULL;
	QStatusBar * status_bar = NULL;

	SlavGPS::LayersPanel * layers_panel = NULL;

	SlavGPS::Viewport * viewport = NULL;
};




#endif /* #ifndef _SG_WINDOW_QT_H_ */
