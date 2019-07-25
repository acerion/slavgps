#ifndef _SG_VIEWPORT_TO_IMAGE_H_
#define _SG_VIEWPORT_TO_IMAGE_H_




#include <QLabel>
#include <QSpinBox>
#include <QPushButton>




#include "dialog.h"
#include "widget_radio_group.h"
#include "viewport_zoom.h"




namespace SlavGPS {




	class GisViewport;
	class Window;




	class ViewportToImage {
	public:
		enum class FileFormat {
			PNG  = 0,
			JPEG = 1
		};

		enum class SaveMode {
			File, /* png or jpeg. */
			Directory,
			FileKMZ,
		};

		ViewportToImage(GisViewport * gisview, ViewportToImage::SaveMode save_mode, Window * window);
		~ViewportToImage();

		bool run_config_dialog(const QString & title);

		/**
		   Run target file/directory selection dialog. Save
		   viewport if user selects target location.

		   @return true if save has been made
		   @return false if save has not been made
		*/
		bool run_save_dialog_and_save(void);

		QString get_destination_full_path(void);
		sg_ret save_to_destination(const QString & full_path);

	private:
		sg_ret save_to_image(const QString & full_path);
		sg_ret save_to_dir(const QString & full_path);

		GisViewport * gisview = NULL;
		ViewportToImage::SaveMode save_mode;
		Window * window = NULL;

		/* For storing last selections. */
		int scaled_total_width = 0;    /* Width of target image. */
		int scaled_total_height = 0;   /* Height of target image. */
		ViewportToImage::FileFormat file_format = ViewportToImage::FileFormat::JPEG;
		int n_tiles_x = 0;
		int n_tiles_y = 0;

		VikingScale original_viking_scale;  /* Viking scale of original viewport. */
		VikingScale scaled_viking_scale;    /* Viking scale of scaled viewport. */
	};




	class ViewportSaveDialog : public BasicDialog {
		Q_OBJECT
	public:
		ViewportSaveDialog(QString const & title, GisViewport * gisview, QWidget * parent = NULL);
		~ViewportSaveDialog();

		void build_ui(ViewportToImage::SaveMode save_mode, ViewportToImage::FileFormat file_format);
		void get_scaled_parameters(int & width, int & height, VikingScale & scale) const;
		ViewportToImage::FileFormat get_image_format(void) const;

		int get_n_tiles_x(void) const;
		int get_n_tiles_y(void) const;


	private slots:
		void accept_cb(void);
		void get_size_from_viewport_cb(void);
		void calculate_total_area_cb();

		void handle_changed_width_cb(int w);
		void handle_changed_height_cb(int h);

	private:
		GisViewport * gisview = NULL;

		QSpinBox * width_spin = NULL;
		QSpinBox * height_spin = NULL;
		QLabel * total_area_label = NULL;
		RadioGroupWidget * output_format_radios = NULL;

		/* Proportion of width/height dimensions of viewport
		   (original viewport and scaled viewport).
		   p = w/h. */
		double original_proportion = 0.0;

		/* Width/height of original viewport. */
		int original_total_width = 0;
		int original_total_height = 0;

		/* Viking scale of original viewport. */
		VikingScale original_viking_scale;

		/* Only used for ViewportToImage::SaveMode::Directory. */
		QSpinBox * tiles_width_spin = NULL;
		QSpinBox * tiles_height_spin = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_TO_IMAGE_H_ */
