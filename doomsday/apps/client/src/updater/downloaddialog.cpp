/** @file downloaddialog.cpp Dialog that downloads a distribution package.
 * @ingroup updater
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <QFile>
#include "de_platform.h"
#include "updater/downloaddialog.h"
#include "updater/updatersettings.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/clientwindow.h"
#include "dd_version.h"
#include "network/net_main.h"

#include <de/ProgressWidget>
#include <de/SignalAction>
#include <de/Log>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDir>
#include <QUrl>

#ifdef WIN32
#  undef open
#endif

using namespace de;

static DownloadDialog *downloadInProgress;

DENG2_PIMPL(DownloadDialog)
{
    enum State {
        Connecting,
        MaybeRedirected,
        Downloading,
        Finished,
        Error
    };
    State state;

    QNetworkAccessManager* network;
    ProgressWidget *progress;
    QUrl uri;
    QUrl uri2;
    NativePath savedFilePath;
    QNetworkReply *reply;
    String redirected;
    dint64 receivedBytes;
    dint64 totalBytes;
    String location;
    String errorMessage;

    Impl(Public *d, String downloadUri, String fallbackUri)
        : Base(d), state(Connecting), uri(downloadUri), uri2(fallbackUri), reply(0),
          receivedBytes(0), totalBytes(0)
    {
        ScrollAreaWidget &area = self().area();

        progress = new ProgressWidget;
        area.add(progress);
        progress->setImageScale(toDevicePixels(.4f));
        progress->setAlignment(ui::AlignLeft);
        progress->setSizePolicy(ui::Fixed, ui::Expand);
        progress->setRange(Rangei(0, 100));
        progress->rule()
                .setLeftTop(area.contentRule().left(), area.contentRule().top())
                .setInput(Rule::Width, self().rule("dialog.download.width"));

        area.setContentSize(progress->rule().width(), progress->rule().height());

        self().buttons() << new DialogButtonItem(DialogWidget::Reject,
                                               tr("Cancel Download"),
                                               new SignalAction(thisPublic, SLOT(cancel())));

        updateLocation(uri);
        updateProgress();

        network = new QNetworkAccessManager(thisPublic);
        QObject::connect(network, SIGNAL(finished(QNetworkReply *)), thisPublic, SLOT(finished(QNetworkReply *)));

        startDownload();
    }

    void updateLocation(QUrl const &url)
    {
        location = url.host();
        updateProgress();
    }

    void startDownload()
    {
        state = Connecting;
        redirected.clear();

        String path = uri.path();
        QDir::current().mkpath(UpdaterSettings().downloadPath()); // may not exist
        savedFilePath = UpdaterSettings().downloadPath() / path.fileName();

        QNetworkRequest request(uri);
        request.setRawHeader("User-Agent", Net_UserAgent().toLatin1());
        reply = network->get(request);

        QObject::connect(reply, SIGNAL(metaDataChanged()), thisPublic, SLOT(replyMetaDataChanged()));
        QObject::connect(reply, SIGNAL(downloadProgress(qint64,qint64)), thisPublic, SLOT(progress(qint64,qint64)));

        LOG_NOTE("Downloading %s, saving as: %s") << uri.toString() << savedFilePath;

        // Global state "flag".
        downloadInProgress = thisPublic;
    }

    void updateProgress()
    {
        String fn = savedFilePath.fileName();
        String msg;

        const double MB = 1.0e6; // MiB would be 2^20

        switch (state)
        {
        default:
            msg = String(tr("Connecting to %1")).arg(_E(b) + location + _E(.));
            break;

        case Downloading:
            msg = String(tr("Downloading %1 (%2 MB) from %3"))
                    .arg(_E(b) + fn + _E(.)).arg(totalBytes / MB, 0, 'f', 1).arg(location);
            break;

        case Finished:
            msg = String(tr("Ready to install\n%1")).arg(_E(b) + fn + _E(.));
            break;

        case Error:
            msg = String(tr("Failed to download:\n%1")).arg(_E(b) + errorMessage);
            break;
        }

        progress->setText(msg);
    }
};

DownloadDialog::DownloadDialog(String downloadUri, String fallbackUri)
    : DialogWidget("download"), d(new Impl(this, downloadUri, fallbackUri))
{}

DownloadDialog::~DownloadDialog()
{
    downloadInProgress = 0;
}

String DownloadDialog::downloadedFilePath() const
{
    if (!isReadyToInstall()) return "";
    return d->savedFilePath;
}

bool DownloadDialog::isReadyToInstall() const
{
    return d->state == Impl::Finished;
}

bool DownloadDialog::isFailed() const
{
    return d->state == Impl::Error;
}

void DownloadDialog::finished(QNetworkReply *reply)
{
    LOG_AS("Download");

    reply->deleteLater();
    d->reply = 0;

    if (reply->error() != QNetworkReply::NoError)
    {
        LOG_WARNING("Failed: ") << reply->errorString();

        d->state = Impl::Error;
        d->errorMessage = reply->errorString();
        d->updateProgress();
        downloadInProgress = 0;
        return;
    }

    /// @todo If/when we include WebKit, this can be done more intelligently using QWebPage. -jk

    if (!d->redirected.isEmpty())
    {
        LOG_NOTE("Redirected to: %s") << d->redirected;
        d->uri = QUrl(d->redirected);
        d->redirected.clear();
        d->startDownload();
        return;
    }

    if (d->state == Impl::MaybeRedirected)
    {
        // This does not look like a binary file... Let's see if we can parse the page.
        QString html = QString::fromUtf8(reply->readAll());

        /// @todo Use a regular expression for parsing the redirection.

        int start = html.indexOf("<meta http-equiv=\"refresh\"", 0, Qt::CaseInsensitive);
        if (start < 0)
        {
            LOG_WARNING("Received an HTML page instead of a binary file");

            // Do we have a fallback option?
            if (!d->uri2.isEmpty() && d->uri2 != d->uri)
            {
                d->uri = d->uri2;
                d->updateLocation(d->uri);
                d->startDownload();
                return;
            }

            emit downloadFailed(d->uri.toString());
            return;
        }
        start = html.indexOf("url=\"", start, Qt::CaseInsensitive);
        if (start < 0)
        {
            emit downloadFailed(d->uri.toString());
            return;
        }
        start += 5; // skip: url="
        QString equivRefresh = html.mid(start, html.indexOf("\"", start) - start);
        equivRefresh.replace("&amp;", "&");

        // This is what we should actually be downloading.
        d->uri = QUrl::fromEncoded(equivRefresh.toLatin1());

        LOG_NOTE("Redirected to: %s") << d->uri.toString();

        d->startDownload();
        return;
    }

    // Save the received data.
    QFile file(d->savedFilePath);
    if (file.open(QFile::WriteOnly | QFile::Truncate))
    {
        file.write(reply->readAll());
    }
    else
    {
        LOG_WARNING("Failed to write to: %s") << d->savedFilePath;
        emit downloadFailed(d->uri.toString());
    }

    buttons().clear()
            << new DialogButtonItem(DialogWidget::Reject, tr("Delete"),
                                    new SignalAction(this, SLOT(cancel())))
            << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Install Update"));

    d->state = Impl::Finished;
    d->progress->setRotationSpeed(0);
    d->updateProgress();

    // Make sure the finished download is noticed by the user.
    showCompletedDownload();

    LOG_DEBUG("Request finished");
}

void DownloadDialog::cancel()
{
    LOG_NOTE("Download cancelled due to user request");

    d->state = Impl::Error;
    d->progress->setRotationSpeed(0);

    if (d->reply)
    {
        d->reply->abort();
        buttons().clear()
                << new DialogButtonItem(DialogWidget::Reject, tr("Close"));
    }
    else
    {
        reject();
    }
}

void DownloadDialog::progress(qint64 received, qint64 total)
{
    LOG_AS("Download");

    if (d->state == Impl::Downloading && total > 0)
    {
        d->totalBytes = total;
        d->receivedBytes = received;
        d->updateProgress();

        dint64 percent = received * 100 / total;
        d->progress->setProgress(percent);

        emit downloadProgress(percent);
    }
}

void DownloadDialog::replyMetaDataChanged()
{
    LOG_AS("Download");

    String contentType = d->reply->header(QNetworkRequest::ContentTypeHeader).toString();
    String redirection = d->reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();

    if (!redirection.isEmpty())
    {
        d->redirected = redirection;
    }
    else if (contentType.startsWith("text/html"))
    {
        // Looks like a redirection page.
        d->state = Impl::MaybeRedirected;
    }
    else
    {
        LOG_DEBUG("Receiving content of type '%s'") << contentType;
        d->state = Impl::Downloading;
    }
}

bool DownloadDialog::isDownloadInProgress()
{
    return downloadInProgress != 0;
}

DownloadDialog &DownloadDialog::currentDownload()
{
    DENG2_ASSERT(isDownloadInProgress());
    return *downloadInProgress;
}

void DownloadDialog::showCompletedDownload()
{
    if (downloadInProgress && downloadInProgress->isReadyToInstall())
    {
        ClientWindow::main().taskBar().openAndPauseGame();
        downloadInProgress->open();
    }
}
