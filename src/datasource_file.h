/*
  This file is part of SlavGPS package.
*/
#ifndef _SG_DATA_SOURCE_FILE_H_
#define _SG_DATA_SOURCE_FILE_H_



#include <QString>
#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QComboBox>

#include "babel.h"
#include "widget_file_entry.h"



class DataSourceFileDialog : public QDialog {
	Q_OBJECT
public:
	DataSourceFileDialog(QString const & title, QWidget * parent = NULL);
	~DataSourceFileDialog();

	void build_ui(void);

private slots:
	void accept_cb(void);

public:
	QWidget * parent = NULL;
	QDialogButtonBox * button_box = NULL;
	QVBoxLayout * vbox = NULL;
	QComboBox * file_types = NULL;

	SlavGPS::SGFileEntry * file_entry = NULL;

	void add_file_filter(BabelFile * babel_file);
};


#endif /* #define _SG_DATA_SOURCE_FILE_H_ */
