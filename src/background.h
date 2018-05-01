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
	class BackgroundJob;
	class BackgroundJob2;




	typedef int (* background_thread_fn)(BackgroundJob *);




	enum class ThreadPoolType {
		REMOTE, /* i.e. Network requests - can have an arbitary large pool. */
		LOCAL,  /* i.e. CPU bound tasks - pool should be no larger than available CPUs for best performance. */
#ifdef HAVE_LIBMAPNIK
		LOCAL_MAPNIK,  /* Due to potential issues with multi-threading a separate configurable pool for Mapnik. */
#endif
	};




	class BackgroundJob {
	public:
		BackgroundJob();
		virtual ~BackgroundJob() {};

		virtual void cleanup_on_cancel(void) {};

		background_thread_fn thread_fn = NULL;
		int n_items = 0;
		bool remove_from_list = false;
		QPersistentModelIndex * index = NULL;
		int progress = 0; /* 0 - 100% */
	};




	class BackgroundJob2 : public QRunnable {
	public:
		BackgroundJob2();
		~BackgroundJob2();

		static void run_in_background(BackgroundJob2 * bg_job, ThreadPoolType pool_type, const QString & job_description);

		virtual void run() = 0; /* Re-implementation of QRunnable::run(). */

		virtual void cleanup_on_cancel(void) {};

		bool set_progress_state(int progress);
		bool test_termination_condition(void);

		int n_items = 0;
		bool remove_from_list = false;
		QPersistentModelIndex * index = NULL;
		int progress = 0; /* 0 - 100% */
	};




	class BackgroundWindow : public QDialog {
		Q_OBJECT

	public:
		BackgroundWindow(QWidget * parent);
		~BackgroundWindow() {};

		void show_window(void);
		QPersistentModelIndex * insert_job(const QString & message, BackgroundJob * bg_job);
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




	class BackgroundWindow2 : public QDialog {
		Q_OBJECT

	public:
		BackgroundWindow2(QWidget * parent);
		~BackgroundWindow2() {};

		void show_window(void);
		QPersistentModelIndex * insert_job(const QString & message, BackgroundJob2 * bg_job);
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




	void a_background_thread(BackgroundJob * bg_job, ThreadPoolType pool_type, const QString & job_description);
	bool a_background_thread_progress(BackgroundJob * bg_job, int progress);
	bool a_background_testcancel(BackgroundJob * bg_job);
	void a_background_show_window();
	void a_background_init();
	void a_background_post_init();
	void a_background_post_init_window(QWidget * parent);
	void a_background_uninit();
	void a_background_add_window(Window * window);
	void a_background_remove_window(Window * window);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_BACKGROUND_H_ */
