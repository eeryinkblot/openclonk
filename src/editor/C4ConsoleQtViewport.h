/*
* OpenClonk, http://www.openclonk.org
*
* Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de/
* Copyright (c) 2013, The OpenClonk Team and contributors
*
* Distributed under the terms of the ISC license; see accompanying file
* "COPYING" for details.
*
* "Clonk" is a registered trademark of Matthes Bender, used with permission.
* See accompanying file "TRADEMARK" for details.
*
* To redistribute this file separately, substitute the full license texts
* for the above references.
*/

/* Player and editor viewports in console */

#ifndef INC_C4ConsoleQtViewport
#define INC_C4ConsoleQtViewport
#ifdef WITH_QT_EDITOR

#include "C4Include.h" // needed for automoc
#include "editor/C4ConsoleGUI.h" // for OpenGL
#include "editor/C4ConsoleQt.h"

// Not covered by the <QtWidgets> umbrella any more: Qt6 moved QOpenGLWidget out
// of QtWidgets into QtOpenGLWidgets. Including it where the base class is used
// works on both, and does not depend on an umbrella header's contents.
#include <QOpenGLWidget>

class C4ConsoleQtViewportView : public QOpenGLWidget
{
	Q_OBJECT

	class C4ConsoleQtViewportDockWidget *dock;
	class C4Viewport *cvp;

private:
	bool IsPlayViewport() const;
	qreal GetDevicePixelRatio();
	void ShowContextMenu(const QPoint &pos);
	void AddSelectObjectContextEntry(C4Object *obj, QMenu *menu);
	void UpdateCursor();

protected:
	void focusInEvent(QFocusEvent * event) override;
	void focusOutEvent(QFocusEvent * event) override;
	void mouseMoveEvent(QMouseEvent *eventMove) override;
	void mousePressEvent(QMouseEvent *eventPress) override;
	void mouseDoubleClickEvent(QMouseEvent *eventPress) override;
	void mouseReleaseEvent(QMouseEvent *releaseEvent) override;
	void wheelEvent(QWheelEvent *event) override;
	void keyPressEvent(QKeyEvent * event) override;
	void keyReleaseEvent(QKeyEvent * event) override;
	// The only signature Qt6 changed rather than removed: QWidget::enterEvent
	// takes a QEnterEvent* from 6.0 on, so this cannot be spelled once for both.
	// The parameter is unused either way.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	void enterEvent(QEnterEvent *) override;
#else
	void enterEvent(QEvent *) override;
#endif
	void leaveEvent(QEvent *) override;

public:
	C4ConsoleQtViewportView(class C4ConsoleQtViewportScrollArea *scrollarea);

	// QOpenGLWidget functions
	void initializeGL() override;
	void resizeGL(int w, int h) override;
	void paintGL() override;
};

class C4ConsoleQtViewportScrollArea : public QAbstractScrollArea
{
	Q_OBJECT
	
	class C4ConsoleQtViewportDockWidget *dock;
	class C4Viewport *cvp;
	int32_t is_updating_scrollbars;

protected:
	void scrollContentsBy(int dx, int dy) override;
	bool viewportEvent(QEvent *e) override;

public:
	C4ConsoleQtViewportScrollArea(class C4ConsoleQtViewportDockWidget *dock);

	void setupViewport(QWidget *viewport) override;
	void ScrollBarsByViewPosition();
	void setScrollBarVisibility(bool visible);

	friend C4ConsoleQtViewportView;
};

class C4ConsoleQtViewportDockWidget : public QDockWidget
{
	Q_OBJECT

	class C4ConsoleQtMainWindow *main_window;
	class C4ViewportWindow *cvp;
	C4ConsoleQtViewportView *view;

protected:
	void mousePressEvent(QMouseEvent *eventPress) override;
	void OnActiveChanged(bool active);

public:
	C4ConsoleQtViewportDockWidget(class C4ConsoleQtMainWindow *main_window, QMainWindow *parent, class C4ViewportWindow *window);
	void closeEvent(QCloseEvent * event) override;
	class C4ViewportWindow *GetViewportWindow() const { return cvp; }
	void SetFocus() { raise(); view->setFocus(); } // forward focus to view
	bool HasFocus() { return view->hasFocus(); } // forward focus to view
	bool event(QEvent *e) override;

private slots :
	void TopLevelChanged(bool is_floating);

	friend C4ConsoleQtViewportView;
	friend C4ConsoleQtViewportScrollArea;
};


#endif // WITH_QT_EDITOR
#endif // INC_C4ConsoleQtViewport
