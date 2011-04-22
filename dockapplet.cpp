#include "dockapplet.h"

#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtGui/QGraphicsScene>
#include "textgraphicsitem.h"
#include "panelapplication.h"
#include "panelwindow.h"
#include "x11support.h"

ClientGraphicsItem::ClientGraphicsItem(Client* client)
	: m_client(client)
{

}

ClientGraphicsItem::~ClientGraphicsItem()
{

}

QRectF ClientGraphicsItem::boundingRect() const
{
	return QRectF(0.0, 0.0, m_client->size().width() - 1, m_client->size().height() - 1);
}

void ClientGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->setPen(Qt::NoPen);

	static const int borderThickness = 3;

	{
		QLinearGradient gradient(0.0, 0.0, borderThickness, 0.0);
		gradient.setSpread(QGradient::RepeatSpread);
		gradient.setColorAt(0, QColor(255, 255, 255, 128));
		gradient.setColorAt(1, QColor(255, 255, 255, 0));
		painter->setBrush(QBrush(gradient));
		painter->drawRect(0.0, 8.0, borderThickness, m_client->size().height() - 16.0);
	}

	{
		QLinearGradient gradient(m_client->size().width() - borderThickness, 0.0, m_client->size().width(), 0.0);
		gradient.setSpread(QGradient::RepeatSpread);
		gradient.setColorAt(0, QColor(255, 255, 255, 0));
		gradient.setColorAt(1, QColor(255, 255, 255, 128));
		painter->setBrush(QBrush(gradient));
		painter->drawRect(m_client->size().width() - borderThickness, 8.0, borderThickness, m_client->size().height() - 16.0);
	}
}

Client::Client(DockApplet* dockApplet, unsigned long handle)
{
	m_dockApplet = dockApplet;
	m_handle = handle;

	updateName();
	updateVisibility();

	m_clientItem = new ClientGraphicsItem(this);

	m_textItem = new TextGraphicsItem(m_clientItem);
	m_textItem->setColor(Qt::white);
	m_textItem->setFont(m_dockApplet->panelWindow()->font());
	m_textItem->setPos(8.0, m_dockApplet->panelWindow()->textBaseLine());

	m_dockApplet->registerClient(this);
	m_dockApplet->updateLayout();
}

Client::~Client()
{
	delete m_textItem;
	delete m_clientItem;

	m_dockApplet->unregisterClient(this);
	m_dockApplet->updateLayout();
}

void Client::removed()
{
	// Call destructor for now.
	// TODO: Animations.
	delete this;
}

QSize Client::desiredSize()
{
	return QSize(256, 256); // TODO: Make it configurable;
}

void Client::setPosition(const QPoint& position)
{
	m_clientItem->setPos(position.x(), position.y());
}

void Client::setSize(const QSize& size)
{
	m_size = size;
	updateLayout();
}

void Client::updateLayout()
{
	if(!m_visible)
	{
		m_clientItem->setParentItem(NULL);
		return;
	}

	m_clientItem->setParentItem(m_dockApplet->appletItem());

	QFontMetrics fontMetrics(m_textItem->font());
	QString shortName = fontMetrics.elidedText(m_name, Qt::ElideRight, m_size.width() - 16);
	m_textItem->setText(shortName);
}

void Client::updateName()
{
	m_name = X11Support::instance()->getWindowName(m_handle);
}

void Client::updateVisibility()
{
	QVector<unsigned long> windowTypes = X11Support::instance()->getWindowPropertyAtomsArray(m_handle, "_NET_WM_WINDOW_TYPE");
	m_visible = windowTypes.contains(X11Support::instance()->atom("_NET_WM_WINDOW_TYPE_NORMAL"));
	QVector<unsigned long> windowStates = X11Support::instance()->getWindowPropertyAtomsArray(m_handle, "_NET_WM_STATE");
	if(windowStates.contains(X11Support::instance()->atom("_NET_WM_STATE_SKIP_TASKBAR")))
		m_visible = false;
}

DockApplet::DockApplet(PanelWindow* panelWindow)
	: Applet(panelWindow)
{
	// Register for notifications about client list changes.
	connect(PanelApplication::instance(), SIGNAL(clientListChanged()), this, SLOT(clientListChanged()));
}

DockApplet::~DockApplet()
{
}

bool DockApplet::init()
{
	// Get info about existing clients.
	clientListChanged();

	return true;
}

void DockApplet::updateLayout()
{
	// TODO: Vertical orientation support.

	int numVisibleClients = 0;
	for(int i = 0; i < m_clients.size(); i++)
	{
		if(m_clients[i]->isVisible())
			numVisibleClients++;
	}

	int freeSpace = m_size.width();
	int spaceForOneClient = (numVisibleClients > 0) ? freeSpace/numVisibleClients : 0;
	int currentPosition = 0;
	for(int i = 0; i < m_clients.size(); i++)
	{
		if(!m_clients[i]->isVisible())
			continue;

		int spaceForThisClient = spaceForOneClient;
		if(m_clients[i]->desiredSize().width() < spaceForThisClient)
			spaceForThisClient = m_clients[i]->desiredSize().width();
		m_clients[i]->setPosition(QPoint(currentPosition, 0));
		m_clients[i]->setSize(QSize(spaceForThisClient - 4, m_size.height()));
		currentPosition += spaceForThisClient;
	}

	m_appletItem->update();
}

void DockApplet::layoutChanged()
{
	updateLayout();
}

QSize DockApplet::desiredSize()
{
	return QSize(-1, -1); // Take all available space.
}

void DockApplet::registerClient(Client* client)
{
	m_clients.append(client);
}

void DockApplet::unregisterClient(Client* client)
{
	m_clients.remove(m_clients.indexOf(client));
}

void DockApplet::clientListChanged()
{
	QVector<unsigned long> windows = X11Support::instance()->getWindowPropertyWindowsArray(X11Support::instance()->rootWindow(), "_NET_CLIENT_LIST");

	// Handle new clients.
	for(int i = 0; i < windows.size(); i++)
	{
		bool found = false;
		for(int k = 0; k < m_clients.size(); k++)
		{
			if(m_clients[k]->handle() == windows[i])
			{
				found = true;
				break;
			}
		}

		if(!found)
		{
			Client* client = new Client(this, windows[i]);
		}
	}

	// Handle removed clients.
	for(int i = 0; i < m_clients.size(); i++)
	{
		bool found = false;
		for(int k = 0; k < windows.size(); k++)
		{
			if(m_clients[i]->handle() == windows[k])
			{
				found = true;
				break;
			}
		}

		if(!found)
		{
			m_clients[i]->removed();
		}
	}
}
