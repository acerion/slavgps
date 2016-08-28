#ifndef _SG_WINDOW_QT_H_
#define _SG_WINDOW_QT_H_




#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>




class Window : public QMainWindow {
	Q_OBJECT

public:
	Window();


protected:

private slots:

private:

	void create_ui(void);

	QMenuBar * menu_bar = NULL;
	QToolBar * tool_bar = NULL;
	QStatusBar * status_bar = NULL;

};




#endif /* #ifndef _SG_WINDOW_QT_H_ */
