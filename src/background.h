/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_BACKGROUND_H_
#define _SG_BACKGROUND_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdint>

#include <QObject>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QTableView>
#include <QRunnable>




namespace SlavGPS {




	class Window;




	enum class ThreadPoolType {
		REMOTE, /* i.e. Network requests - can have an arbitary large pool. */
		LOCAL,  /* i.e. CPU bound tasks - pool should be no larger than available CPUs for best performance. */
#ifdef HAVE_LIBMAPNIK
		LOCAL_MAPNIK,  /* Due to potential issues with multi-threading a separate configurable pool for Mapnik. */
#endif
	};




	class BackgroundJob : public QRunnable {
	public:
		BackgroundJob() {};
		~BackgroundJob();

		virtual void run() {}; /* Re-implementation of QRunnable::run(). TODO: make this function pure virtual in future. */

		virtual void cleanup_on_cancel(void) {};

		void set_description(const QString & job_description) { this->description = job_description; };
		bool set_progress_state(int progress);
		bool test_termination_condition(void);

		int n_items = 0;
		bool remove_from_list = false;
		QPersistentModelIndex * index = NULL;
		int progress = 0; /* 0 - 100% */

		QString description;
	};




	class BackgroundWindow : public QDialog {
		Q_OBJECT
	public:
		BackgroundWindow(QWidget * parent);
		~BackgroundWindow() {};

		void show_window(void);
		QPersistentModelIndex * insert_job(BackgroundJob * bg_job);
		void remove_job(QStandardItem * item);

		QStandardItemModel * model = NULL;
		QTableView * view = NULL;


	private slots:
		void close_cb(void);
		void remove_selected_cb(void);
		void remove_all_cb(void);
		void remove_selected_state_cb(void);

	private:

		QDialogButtonBox * button_box = NULL;
		QPushButton * close = NULL;
		QPushButton * remove_selected = NULL;
		QPushButton * remove_all = NULL;
		QVBoxLayout * vbox = NULL;
	};




	class Background {
	public:
		static void init();
		static void post_init();
		static void post_init_window(QWidget * parent);
		static void uninit(void);

		static void run_in_background(BackgroundJob * bg_job, ThreadPoolType pool_type); /* TODO: should this become a non-static method of BackgroundJob? */

		static bool test_termination_condition(void);

		static void show_window();
		static void add_window(Window * window);
		static void remove_window(Window * window);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_BACKGROUND_H_ */
