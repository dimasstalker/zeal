/****************************************************************************
**
** Copyright (C) 2015-2016 Oleg Shparber
** Copyright (C) 2013-2014 Jerzy Kozera
** Contact: https://go.zealdocs.org/l/contact
**
** This file is part of Zeal.
**
** Zeal is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** Zeal is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Zeal. If not, see <https://www.gnu.org/licenses/>.
**
****************************************************************************/

#include "webview.h"

#include "webviewtab.h"
#include "../mainwindow.h"

#include <core/application.h>

#include <QApplication>
#include <QDesktopServices>
#include <QMenu>
#include <QWheelEvent>

using namespace Zeal::WidgetUi;

WebView::WebView(QWidget *parent)
    : QWebView(parent)
{
    setAttribute(Qt::WA_AcceptTouchEvents, false);
    page()->setLinkDelegationPolicy(QWebPage::DelegateExternalLinks);
    page()->setNetworkAccessManager(Core::Application::instance()->networkManager());
}

int WebView::zoomLevel() const
{
    return m_zoomLevel;
}

void WebView::setZoomLevel(int level)
{
    if (level == m_zoomLevel) {
        return;
    }

    level = qMax(0, level);
    level = qMin(level, availableZoomLevels().size() - 1);

    m_zoomLevel = level;

    setZoomFactor(availableZoomLevels().at(level) / 100.0);
    emit zoomLevelChanged();
}

const QVector<int> &WebView::availableZoomLevels()
{
    static const QVector<int> zoomLevels = {30, 40, 50, 67, 80, 90, 100,
                                            110, 120, 133, 150, 170, 200,
                                            220, 233, 250, 270, 285, 300};
    return zoomLevels;
}

const int WebView::defaultZoomLevel()
{
    static const int level = availableZoomLevels().indexOf(100);
    return level;
}

void WebView::zoomIn()
{
    setZoomLevel(m_zoomLevel + 1);
}

void WebView::zoomOut()
{
    setZoomLevel(m_zoomLevel - 1);
}

void WebView::resetZoom()
{
    setZoomLevel(defaultZoomLevel());
}

QWebView *WebView::createWindow(QWebPage::WebWindowType type)
{
    Q_UNUSED(type)

    MainWindow *mainWindow = qobject_cast<MainWindow *>(qApp->activeWindow());
    return mainWindow->createTab()->m_webView;
}

void WebView::contextMenuEvent(QContextMenuEvent *event)
{
#if QT_VERSION >= 0x050600
    const QWebHitTestResult hitTestResult = hitTestContent(event->pos());

    // Return standard menu for input fields.
    if (hitTestResult.isContentEditable()) {
         QWebView::contextMenuEvent(event);
         return;
    }

    event->accept();

    if (m_contextMenu) {
        m_contextMenu->deleteLater();
    }

    QMenu *m_contextMenu = new QMenu(this);

    const QUrl linkUrl = hitTestResult.linkUrl();
    const QString linkText = hitTestResult.linkText();

    if (linkUrl.isValid()) {
        m_contextMenu->addAction(tr("Open Link in New Tab"), this, [this]() {
            triggerPageAction(QWebPage::WebAction::OpenLinkInNewWindow);
        });

        if (linkUrl.scheme() != QLatin1String("qrc")) {
            m_contextMenu->addAction(tr("Open Link in Desktop Browser"), this, [linkUrl]() {
                QDesktopServices::openUrl(linkUrl);
            });

            m_contextMenu->addAction(pageAction(QWebPage::CopyLinkToClipboard));
        }
    }

    if (hitTestResult.isContentSelected()) {
        if (!m_contextMenu->isEmpty()) {
            m_contextMenu->addSeparator();
        }

        m_contextMenu->addAction(pageAction(QWebPage::Copy));
    }

    if (!linkUrl.isValid() && url().scheme() != QLatin1String("qrc")) {
        if (!m_contextMenu->isEmpty()) {
            m_contextMenu->addSeparator();
        }

        m_contextMenu->addAction(tr("Open Page in Desktop Browser"), this, [this]() {
            QDesktopServices::openUrl(url());
        });
    }

    if (m_contextMenu->isEmpty()) {
        return;
    }

    m_contextMenu->popup(event->globalPos());
#else
    QWebView::contextMenuEvent(event);
#endif
}

void WebView::mousePressEvent(QMouseEvent *event)
{
    switch (event->button()) {
    case Qt::BackButton:
        back();
        event->accept();
        return;
    case Qt::ForwardButton:
        forward();
        event->accept();
        return;
    case Qt::LeftButton:
        if (!(event->modifiers() & Qt::ControlModifier || event->modifiers() & Qt::ShiftModifier)) {
            break;
        }
    case Qt::MiddleButton:
        m_clickedLink = hitTestContent(event->pos()).linkUrl();
        if (!m_clickedLink.isValid()) {
            break;
        }

        event->accept();
        return;
    default:
        break;
    }

    QWebView::mousePressEvent(event);
}

void WebView::mouseReleaseEvent(QMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        if (!(event->modifiers() & Qt::ControlModifier || event->modifiers() & Qt::ShiftModifier)) {
            break;
        }
    case Qt::MiddleButton:
        if (!m_clickedLink.isEmpty() && m_clickedLink == hitTestContent(event->pos()).linkUrl()) {
            QWebView *webView = createWindow(QWebPage::WebBrowserWindow);
            webView->load(m_clickedLink);
            event->accept();
            return;
        }

        break;
    default:
        break;
    }

    QWebView::mouseReleaseEvent(event);
}

void WebView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const QPoint angleDelta = event->angleDelta();
        int delta = qAbs(angleDelta.x()) > qAbs(angleDelta.y()) ? angleDelta.x() : angleDelta.y();
        const int direction = delta > 0 ? 1 : -1;

        int levelDelta = 0;
        while (delta * direction >= 120) {
            levelDelta += direction;
            delta -= 120 * direction;
        }

        setZoomLevel(m_zoomLevel + levelDelta);
        event->accept();
        return;
    }

    QWebView::wheelEvent(event);
}

QWebHitTestResult WebView::hitTestContent(const QPoint &pos) const
{
    return page()->mainFrame()->hitTestContent(pos);
}
