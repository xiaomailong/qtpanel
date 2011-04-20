#include "panelapplication.h"

#include "x11support.h"

PanelApplication::PanelApplication(int& argc, char** argv)
	: QApplication(argc, argv)
{
	m_x11support = new X11Support();

	m_panelWindow = new PanelWindow();
	m_panelWindow->resize(128, 30);
	m_panelWindow->setLayoutPolicy(PanelWindow::FillSpace);
	//m_panelWindow->setVerticalAnchor(PanelWindow::Max);
	m_panelWindow->setDockMode(true);
	m_panelWindow->init();
	m_panelWindow->show();
}

PanelApplication::~PanelApplication()
{
	delete m_panelWindow;
	delete m_x11support;
}
