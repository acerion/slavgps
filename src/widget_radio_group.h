/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _SG_WIDGET_RADIO_GROUP_H_
#define _SG_WIDGET_RADIO_GROUP_H_




#include <list>
#include <cstdint>

#include <QObject>
#include <QString>
#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QButtonGroup>




namespace SlavGPS {




	class SGRadioGroup : public QGroupBox {
		Q_OBJECT
	public:
		SGRadioGroup(QString & title, std::list<QString> & labels, QWidget * parent);
		~SGRadioGroup();

		uint32_t value(void);

		//std::list<char *> * get_list(void);

	signals:

	private slots:
		//void add_file();
		//void del_file();


	private:
		QVBoxLayout * vbox = NULL;
		QButtonGroup * group = NULL;
		//std::list<char *> * file_list = NULL;
		//QDialogButtonBox * button_box = NULL;
		//QPushButton * add = NULL;
		//QPushButton * del = NULL;
		//QVBoxLayout * vbox = NULL;

		//QStandardItemModel * model = NULL;
		//QTableView * view = NULL;

		//QFileDialog * file_selector = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_RADIO_GROUP_H_ */