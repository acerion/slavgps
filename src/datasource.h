/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012, Rob Norris <rw_norris@hotmail.com>
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
 */

#ifndef _SG_DATASOURCE_H_
#define _SG_DATASOURCE_H_




#include <QLabel>
#include <QString>




#include "dialog.h"




namespace SlavGPS {




	class AcquireOptions;
	class DownloadOptions;
	class LayerTRW;
	class Track;
	class AcquireContext;
	class AcquireProgressDialog;
	enum class AcquireProgressCode;
	class ListSelectionWidget;




	enum class TargetLayerMode {
		/* Generally Datasources shouldn't use these and let the HCI decide between the create or add to layer options. */
		CreateNewLayer,
		AddToLayer,
		AutoLayerManagement,
		ManualLayerManagement,
	};
	/* TODO_MAYBE: replace track/layer? */




	class DataSource {
	public:
		DataSource() {};
		virtual ~DataSource();

		virtual LoadStatus acquire_into_layer(__attribute__((unused)) AcquireContext & acquire_context, __attribute__((unused)) AcquireProgressDialog * progr_dialog) { return LoadStatus::Code::Error; };
		virtual void progress_func(__attribute__((unused)) AcquireProgressCode code, __attribute__((unused)) void * data, __attribute__((unused)) AcquireContext * acquire_context) { return; };
		virtual void cleanup(__attribute__((unused)) void * data) { return; };
		virtual int kill(__attribute__((unused)) const QString & status) { return -1; };

		virtual sg_ret on_complete(void) { return sg_ret::ok; };

		virtual int run_config_dialog(__attribute__((unused)) AcquireContext & acquire_context) { return QDialog::Rejected; };

		virtual AcquireProgressDialog * create_progress_dialog(const QString & title);

		/* ID unique for every type of data source. */
		virtual SGObjectTypeID get_source_id(void) const = 0;

		QString m_window_title;
		QString m_layer_title;

		TargetLayerMode m_layer_mode;
		bool m_autoview = false;

		/* After failure the dialog will be always kept
		   open. But how the dialog window should behave on
		   successful completion of task? */
		bool m_keep_dialog_open_after_success = false;

		AcquireOptions * m_acquire_options = nullptr;
		DownloadOptions * m_download_options = nullptr;
	};




	class DataSourceDialog : public BasicDialog {
		Q_OBJECT
	public:
		DataSourceDialog(const QString & window_title, QWidget * parent_widget = nullptr) : BasicDialog(parent_widget) { this->setWindowTitle(window_title); };
		virtual AcquireOptions * create_acquire_options(__attribute__((unused)) AcquireContext & acquire_context) { return nullptr; };
	};




	class AcquireProgressDialog : public BasicDialog {
		Q_OBJECT
	public:
		AcquireProgressDialog(const QString & window_title, bool keep_open_after_success, QWidget * parent = nullptr);
		~AcquireProgressDialog();

		void set_headline(const QString & text);
		void set_current_status(const QString & text);

		ListSelectionWidget * list_selection_widget = nullptr;

	public slots:
		void handle_acquire_completed_with_success_cb(void);
		void handle_acquire_completed_with_failure_cb(void);

	private:
		bool m_keep_dialog_open_after_success = false;
		QLabel * headline = nullptr;
		QLabel * current_status = nullptr;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_H_ */
