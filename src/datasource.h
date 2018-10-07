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




	enum class DataSourceInputType {
		None = 0,
		TRWLayer,
		Track,
		TRWLayerTrack
	};




	enum class DataSourceMode {
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

		virtual bool acquire_into_layer(LayerTRW * trw, AcquireContext & acquire_context, AcquireProgressDialog * progr_dialog) { return false; };
		virtual void progress_func(AcquireProgressCode code, void * data, AcquireContext & acquire_context) { return; };
		virtual void cleanup(void * data) { return; };
		virtual int kill(const QString & status) { return -1; };

		virtual int run_config_dialog(AcquireContext & acquire_context) { return QDialog::Rejected; };

		virtual AcquireProgressDialog * create_progress_dialog(const QString & title);

		QString window_title;
		QString layer_title;


		DataSourceMode mode;
		DataSourceInputType input_type;

		bool autoview = false;
		bool keep_dialog_open = false; /* ... when done. */
		bool is_thread = false;

		AcquireOptions * acquire_options = NULL;
		DownloadOptions * download_options = NULL;
	};




	class DataSourceDialog : public BasicDialog {
		Q_OBJECT
	public:
		DataSourceDialog(const QString & window_title) { this->setWindowTitle(window_title); };

		AcquireOptions * create_acquire_options_layer(LayerTRW * trw);
		AcquireOptions * create_acquire_options_layer_track(LayerTRW * trw, Track * trk);
		AcquireOptions * create_acquire_options_track(Track * trk);
		AcquireOptions * create_acquire_options_none(void);

	private:
		virtual AcquireOptions * get_acquire_options_none(void) { return NULL; };
		virtual AcquireOptions * get_acquire_options_layer_track(const QString & input_layer_filename, const QString & input_track_filename) { return NULL; };
		virtual AcquireOptions * get_acquire_options_layer(const QString & input_layer_filename) { return NULL; };

	};




	class AcquireProgressDialog : public BasicDialog {
		Q_OBJECT
	public:
		AcquireProgressDialog(const QString & window_title, QWidget * parent = NULL);

		void set_headline(const QString & text);
		void set_current_status(const QString & text);
	private:
		QLabel * headline = NULL;
		QLabel * current_status = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_H_ */
