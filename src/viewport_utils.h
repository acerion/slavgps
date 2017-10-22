#ifndef _SG_VIEWPORT_UTILS_H_
#define _SG_VIEWPORT_UTILS_H_




#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>

#include "widget_radio_group.h"




namespace SlavGPS {




	enum class ViewportSaveMode {
		FILE, /* png or jpeg. */
		DIRECTORY,
		FILE_KMZ,
	};




	class Viewport;




	class ViewportToImageDialog : public QDialog {
		Q_OBJECT
	public:
		ViewportToImageDialog(QString const & title, Viewport * viewport, QWidget * parent = NULL);
		~ViewportToImageDialog();

		void build_ui(ViewportSaveMode mode);

	private slots:
		void accept_cb(void);
		void get_size_from_viewport_cb(void);
		void calculate_total_area_cb();

	public:
		Viewport * viewport = NULL;
		QWidget * parent = NULL;
		QDialogButtonBox * button_box = NULL;
		QVBoxLayout * vbox = NULL;

		QSpinBox * width_spin = NULL;
		QSpinBox * height_spin = NULL;

		/* Only used for ViewportSaveMode::DIRECTORY. */
		QSpinBox * tiles_width_spin = NULL;
		QSpinBox * tiles_height_spin = NULL;

		QPushButton use_current_area_button;
		QComboBox * zoom_combo = NULL;
		QLabel total_area_label;

		SGRadioGroup * output_format_radios = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_UTILS_H_ */
