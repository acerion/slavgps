#ifndef _SG_VIEWPORT_SAVE_DIALOG_H_
#define _SG_VIEWPORT_SAVE_DIALOG_H_




#include <QLabel>
#include <QSpinBox>
#include <QPushButton>




#include "dialog.h"
#include "widget_radio_group.h"




namespace SlavGPS {




	enum class ViewportSaveMode {
		File, /* png or jpeg. */
		Directory,
		FileKMZ,
	};




	enum class ViewportSaveFormat {
		PNG  = 0,
		JPEG = 1
	};




	class Viewport;




	class ViewportSaveDialog : public BasicDialog {
		Q_OBJECT
	public:
		ViewportSaveDialog(QString const & title, Viewport * viewport, QWidget * parent = NULL);
		~ViewportSaveDialog();

		void build_ui(ViewportSaveMode mode);
		int get_width(void) const;
		int get_height(void) const;
		ViewportSaveFormat get_image_format(void) const;

	private slots:
		void accept_cb(void);
		void get_size_from_viewport_cb(void);
		void calculate_total_area_cb();

	private:
		Viewport * viewport = NULL;
		QWidget * parent = NULL;

		QSpinBox * width_spin = NULL;
		QSpinBox * height_spin = NULL;
		QLabel * total_area_label = NULL;
		SGRadioGroup * output_format_radios = NULL;

	public:
		/* Only used for ViewportSaveMode::Directory. */
		QSpinBox * tiles_width_spin = NULL;
		QSpinBox * tiles_height_spin = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_SAVE_DIALOG_H_ */
