#ifndef SLAV_QT_H
#define SLAV_QT_H


/* Mock definitions used during migration of SlavGPS to QT. */



#ifdef SLAVGPS_QT
typedef int GdkGC;
typedef int GdkFont;
typedef int GdkColor;
typedef int GtkWindow;
typedef int GtkWidget;
typedef void GdkPixbuf;
typedef void GdkFunction;
typedef int PangoLayout;
typedef int GdkPixmap;
typedef int GtkDrawingArea;
typedef int VikLayerParam;
typedef int VikLayerParamData;
typedef int VikLayerParamType;
typedef int TreeView;
typedef void GObject;
typedef int ui_change_values;
typedef int GtkTreeIter;
typedef int GtkMenu;
typedef int TreeItemType;
typedef int GdkEventButton;
typedef int GdkEventMotion;
typedef int GtkTreePath;
typedef int GdkEventKey;
typedef struct { int*a; int*b; int*c; int*d; int*e; int f; } GtkRadioActionEntry;
typedef int GdkPixdata;
typedef int GdkCursor;
typedef int GdkCursorType;



#define GDK_CURSOR_IS_PIXMAP 1

namespace SlavGPS {

	typedef int TreeItemType;

}

#endif

#endif
