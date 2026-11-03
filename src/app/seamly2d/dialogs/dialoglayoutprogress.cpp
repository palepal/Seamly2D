/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2017  Seamly, LLC                                       *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                             *
 *                                                                         *
 ***************************************************************************
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 **************************************************************************

 ************************************************************************
 **
 **  @file   dialoglayoutprogress.cpp
 **  @author Roman Telezhynskyi <dismine(at)gmail.com>
 **  @date   14 1, 2015
 **
 **  @brief
 **  @copyright
 **  This source code is part of the Valentine project, a pattern making
 **  program, whose allow create and modeling patterns of clothing.
 **  Copyright (C) 2013-2015 Seamly2D project
 **  <https://github.com/fashionfreedom/seamly2d> All Rights Reserved.
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#include "dialoglayoutprogress.h"
#include "ui_dialoglayoutprogress.h"
#include "../options.h"
#include "../core/application_2d.h"

#include <QMessageBox>
#include <QPushButton>
#include <QMovie>
#include <QtDebug>

//---------------------------------------------------------------------------------------------------------------------
DialogLayoutProgress::DialogLayoutProgress(int count, QWidget *parent)
    :QDialog(parent), ui(new Ui::DialogLayoutProgress), maxCount(count), movie(nullptr), isInitialized(false)
{
    ui->setupUi(this);

    qApp->Seamly2DSettings()->getOsSeparator() ? setLocale(QLocale()) : setLocale(QLocale::c());

    ui->progressBar->setMaximum(maxCount);
    ui->progressBar->setValue(0);

    ui->labelMessage->setText(tr("Arranged workpieces: %1 from %2").arg(0).arg(count));

    movie = new QMovie("://icon/16x16/progress.gif");
    ui->labelProgress->setMovie (movie);
    movie->start ();

    QPushButton *bCancel = ui->buttonBox->button(QDialogButtonBox::Cancel);
    SCASSERT(bCancel != nullptr)
    connect(bCancel, &QPushButton::clicked, this, [this](){emit Abort();});

    connect(ui->finish_PushButton, &QPushButton::clicked, this, [this]()
    {
        // Sparrow takes a few seconds to wrap up
        ui->finish_PushButton->setEnabled(false);
        ui->finish_PushButton->setText(tr("Finishing..."));
        emit Abort();
    });

    this->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
}

//---------------------------------------------------------------------------------------------------------------------
DialogLayoutProgress::~DialogLayoutProgress()
{
    delete ui;
    delete movie;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::Start()
{
    show();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::Arranged(int count)
{
    ui->progressBar->setValue(count);
    ui->labelMessage->setText(tr("Arranged workpieces: %1 from %2").arg(count).arg(maxCount));
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::Error(const LayoutErrors &state)
{
    switch (state)
    {
        case LayoutErrors::NoError:
            return;
        case LayoutErrors::PrepareLayoutError:
            qCritical() << tr("Couldn't prepare data for creation layout");
            break;
        case LayoutErrors::EmptyPaperError:
            qCritical() << tr("One or more pattern pieces are bigger than the paper format you selected. Please select a bigger paper format.");
            break;
        case LayoutErrors::ProcessStoped:
        default:
            break;
    }

    done(QDialog::Rejected);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief optimizing turns the window into a floating card with the Sparrow engine's progress.
/// @param elapsed elapsed time in milliseconds.
/// @param limit time limit in seconds.
/// @param length best layout length so far in pixels, 0 before the first layout.
void DialogLayoutProgress::optimizing(int elapsed, int limit, qreal length)
{
    if (ui->sparrow_Widget->isHidden())
    {
        ui->classic_Widget->hide();
        ui->sparrow_Widget->show();
        layout()->setContentsMargins(0, 0, 0, 0);
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);

        // Seamly violet, as in the tool icons: the logo's color on light themes, a lighter tint on dark ones
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        ui->sparrow_ProgressBar->setStyleSheet(ui->sparrow_ProgressBar->styleSheet()
                                               .replace(QLatin1String("palette(highlight)"),
                                                        dark ? QLatin1String("#a48be0") : QLatin1String("#503584")));
    }

    ui->sparrow_ProgressBar->setMaximum(limit * 1000);
    ui->sparrow_ProgressBar->setValue(qMin(elapsed, limit * 1000));

    QString message = tr("%1 s left").arg(qMax(0, limit - elapsed / 1000));
    if (length > 0)
    {
        const Unit unit = qApp->patternUnit();
        message = tr("Length %1 %2").arg(UnitConvertor(length, Unit::Px, unit), 0, 'f', 1).arg(UnitsToStr(unit, true))
                  + QStringLiteral("  %1  ").arg(QChar(0x00B7)) + message;
    }
    ui->sparrowMessage_Label->setText(message);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief reject makes Esc finish a Sparrow run: closing the card would leave it running with the window unlocked.
void DialogLayoutProgress::reject()
{
    if (ui->sparrow_Widget->isHidden())
    {
        QDialog::reject();
    }
    else if (ui->finish_PushButton->isEnabled())
    {
        ui->finish_PushButton->click();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::Finished()
{
    done(QDialog::Accepted);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::showEvent(QShowEvent *event)
{
    QDialog::showEvent( event );
    if ( event->spontaneous() )
    {
        return;
    }

    if (isInitialized)
    {
        return;
    }
    // do your init stuff here

    if (!ui->sparrow_Widget->isHidden() && parentWidget() != nullptr)
    {
        // float over the bottom of the canvas
        adjustSize();
        const QPoint corner((parentWidget()->width() - width()) / 2, parentWidget()->height() - height() - 32);
        move(parentWidget()->mapToGlobal(corner));
    }

    setMaximumSize(size());
    setMinimumSize(size());

    isInitialized = true;//first show windows are held
}
