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




#include <list>
#include <cstdint>
#include <mutex>




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
	class BackgroundWindow;




	enum class ThreadPoolType {
		Remote, /* i.e. Network requests - can have an arbitary large pool. */
		Local,  /* i.e. CPU bound tasks - pool should be no larger than available CPUs for best performance. */
#ifdef HAVE_LIBMAPNIK
		LocalMapnik,  /* Due to potential issues with multi-threading a separate configurable pool for Mapnik. */
#endif
	};




	class BackgroundJob : public QObject, public QRunnable { /* public QObject must come first on list of base classes. */
		Q_OBJECT
	public:
		BackgroundJob() {};
		~BackgroundJob();

		virtual void run() {}; /* Re-implementation of QRunnable::run(). TODO_2_LATER: make this function pure virtual in future. */

		virtual void cleanup_on_cancel(void) {};

		void run_in_background(ThreadPoolType pool_type);

		void set_description(const QString & job_description) { this->description = job_description; };
		bool set_progress_state(int progress);
		bool test_termination_condition(void);

		void detach_from_window(BackgroundWindow * window);

		int n_items = 0;
		bool remove_from_list = false;
		QPersistentModelIndex * index = NULL;
		int progress = 0; /* 0 - 100% */

		QString description;

		std::mutex mutex;
	};




	class BackgroundWindow : public QDialog {
		Q_OBJECT
	public:
		BackgroundWindow(QWidget * parent);
		~BackgroundWindow() {};

		void show_window(void);
		void append_job(BackgroundJob * bg_job);
		void remove_job(QPersistentModelIndex * index);

		QStandardItemModel * model = NULL;
		QTableView * view = NULL;


	private slots:
		void close_cb(void);
		void remove_selected_cb(void);
		void remove_all_cb(void);
		void remove_selected_state_cb(void);

	private:

		void remove_job(QStandardItem * item);

		QDialogButtonBox * button_box = NULL;
		QPushButton * close = NULL;
		QPushButton * remove_selected = NULL;
		QPushButton * remove_all = NULL;
		QVBoxLayout * vbox = NULL;
	};




	class Background {
	public:
		static void init(void);
		static void post_init(void);
		static void post_init_window(QWidget * parent);
		static void uninit(void);

		static bool test_global_termination_condition(void);

		static void show_window();
		static void add_window(Window * window);
		static void remove_window(Window * window);

		void update_status_indication(void);

		BackgroundWindow * bgwindow = NULL;

		/* Total number of items processed in background.
		   The items may come from different jobs. */
		int n_items = 0;

	private:
		/* List of main (top-level) windows that will be updated (in
		   statusbar) with information about status of background jobs. */
		std::list<Window *> main_windows;



		bool stop_all_threads = false;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_BACKGROUND_H_ */
