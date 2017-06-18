/*
  This file is part of SlavGPS package.
*/
#ifndef _SG_DATA_SOURCE_FILE_H_
#define _SG_DATA_SOURCE_FILE_H_




#include <QString>
#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QComboBox>

#include "babel.h"
#include "widget_file_entry.h"




namespace SlavGPS {




	class DataSourceFileDialog : public QDialog {
		Q_OBJECT
	public:
		DataSourceFileDialog(QString const & title, QWidget * parent = NULL);
		~DataSourceFileDialog();

		void build_ui(void);

	private slots:
		void file_type_changed_cb(int index);

	public:
		QDialogButtonBox * button_box = NULL;
		QVBoxLayout * vbox = NULL;
		QComboBox * file_types_combo = NULL;

		SlavGPS::SGFileEntry * file_entry = NULL;

		void add_file_type_filter(BabelFileType * babel_file_type);
	};




} /* namespace SlavGPS */




#endif /* #define _SG_DATA_SOURCE_FILE_H_ */
