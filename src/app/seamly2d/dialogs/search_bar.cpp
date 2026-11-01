//-----------------------------------------------------------------------------
//  @file   search_bar.cpp
//  @author palepal
//  @date   1 Nov, 2026
//
//  @copyright
//  Copyright (C) 2026 Seamly, LLC
//  https://github.com/fashionfreedom/seamly2d
//
//  @brief
//  Seamly2D is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Seamly2D is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Seamly2D. If not, see <http://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------

#include "search_bar.h"
#include "ui_search_bar.h"

#if defined(Q_OS_MACOS)
#include "../core/macos_dock.h"
#endif

#include <QAbstractItemView>
#include <QCompleter>
#include <QDesktopServices>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QShortcut>
#include <QStandardItemModel>
#include <QTreeView>
#include <QUrl>
#include <QUrlQuery>
#include <QWindow>

#include <algorithm>

namespace
{
const int max_results = 50;
const int bar_height  = 24;
const int bar_width   = 420;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief SearchBar creates the search field in the title bar of window on macOS, elsewhere in the right corner of its
/// menu bar, since other title bars cannot hold widgets.
/// @param window main window that hosts the search field.
SearchBar::SearchBar(QMainWindow *window)
    : QWidget(window)
    , ui(new Ui::SearchBar)
    , m_window(window)
    , m_completer(new QCompleter(this))
    , m_model(new QStandardItemModel(0, 2, this))
    , m_source()
    , m_items()
{
    ui->setupUi(this);
    ui->search_line_edit->addAction(QIcon(":/icon/32x32/find.png"), QLineEdit::LeadingPosition);
    ui->search_line_edit->installEventFilter(this);

    // The themes leave the system placeholder color, which is unreadable when the system and theme brightness differ.
    QPalette palette = ui->search_line_edit->palette();
    QColor placeholder = palette.color(QPalette::Text);
    placeholder.setAlphaF(0.5);
    palette.setColor(QPalette::PlaceholderText, placeholder);
    ui->search_line_edit->setPalette(palette);

    QTreeView *popup = new QTreeView;
    popup->setHeaderHidden(true);
    popup->setRootIsDecorated(false);
    popup->setUniformRowHeights(true);
    popup->header()->setStretchLastSection(false);

    m_completer->setWidget(ui->search_line_edit);
    m_completer->setPopup(popup);
    m_completer->setModel(m_model);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setMaxVisibleItems(15);
    popup->setMinimumWidth(bar_width);

    connect(ui->search_line_edit, &QLineEdit::textChanged, this, &SearchBar::search);
    connect(m_completer, QOverload<const QModelIndex &>::of(&QCompleter::activated), this,
            [this](const QModelIndex &index)
    {
        run(index.data(Qt::UserRole).toInt());
    });

    QShortcut *shortcut = new QShortcut(QKeySequence::Find, window);
    connect(shortcut, &QShortcut::activated, this, [this]()
    {
        ui->search_line_edit->setFocus(Qt::ShortcutFocusReason);
        ui->search_line_edit->selectAll();
    });
    ui->search_line_edit->setToolTip(ui->search_line_edit->toolTip() + QStringLiteral(" (%1)")
                                     .arg(shortcut->key().toString(QKeySequence::NativeText)));

#if defined(Q_OS_MACOS)
    window->setWindowFlag(Qt::ExpandedClientAreaHint);
    window->setWindowFlag(Qt::NoTitleBarBackgroundHint);
    window->installEventFilter(this);
#else
    setFixedWidth(bar_width);
    window->menuBar()->setCornerWidget(this);
#endif
}

//---------------------------------------------------------------------------------------------------------------------
SearchBar::~SearchBar()
{
    delete ui;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief setFileName shows the name of the open pattern file while the field is empty.
/// @param name file name.
void SearchBar::setFileName(const QString &name)
{
    ui->search_line_edit->setPlaceholderText(name);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief setItemSource sets the function that lists everything searchable. It runs at the start of each search.
/// @param source function returning the searchable items.
void SearchBar::setItemSource(const std::function<QVector<SearchItem>()> &source)
{
    m_source = source;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief fuzzyScore scores how well pattern matches text: its characters in order, case insensitive, spaces optional.
///
/// Matches at word starts, consecutive matches, plain substrings and exact names score higher, longer texts lower.
/// @param pattern what the user typed.
/// @param text candidate name.
/// @return score, or -1 if text doesn't contain the characters of pattern in order.
int SearchBar::fuzzyScore(const QString &pattern, const QString &text)
{
    // ponytail: greedy left-to-right match, misses a better later alignment; switch to a DP scorer (fzf v2 style)
    // if rankings feel wrong.
    int score = 0;
    int position = 0;
    int last_match = -2;
    for (const QChar &character : pattern)
    {
        if (character.isSpace())
        {
            continue;
        }
        const QChar lower = character.toLower();
        while (position < text.size() && text.at(position).toLower() != lower)
        {
            ++position;
        }
        if (position == text.size())
        {
            return -1;
        }
        const bool word_start = position == 0 || !text.at(position - 1).isLetterOrNumber() ||
                                (text.at(position).isUpper() && text.at(position - 1).isLower());
        score += 1 + (position == last_match + 1 ? 5 : 0) + (word_start ? 8 : 0);
        last_match = position;
        ++position;
    }

    const int index = text.indexOf(pattern.trimmed(), 0, Qt::CaseInsensitive);
    if (index >= 0)
    {
        score += index == 0 ? 30 : 20;
        score += text.size() == pattern.trimmed().size() ? 50 : 0;
    }
    return score - text.size() / 10;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief eventFilter keeps the field in the title bar, and handles Escape and Down in the field.
/// @param object watched object.
/// @param event event.
/// @return true if the event was handled.
bool SearchBar::eventFilter(QObject *object, QEvent *event)
{
#if defined(Q_OS_MACOS)
    if (object == m_window && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    {
        placeInTitleBar();
    }
#endif
    if (object == ui->search_line_edit)
    {
        if (event->type() == QEvent::KeyPress)
        {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Escape)
            {
                ui->search_line_edit->clear();
                ui->search_line_edit->clearFocus();
                return true;
            }
            if (key == Qt::Key_Down && !m_completer->popup()->isVisible())
            {
                search(ui->search_line_edit->text());
                return true;
            }
        }
    }
    return QWidget::eventFilter(object, event);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief search scores every item against text and shows the best ones, best first, plus a help search entry.
/// @param text what the user typed.
void SearchBar::search(const QString &text)
{
    if (text.trimmed().isEmpty())
    {
        m_completer->popup()->hide();
        return;
    }
    if (!m_completer->popup()->isVisible() && m_source)
    {
        m_items = m_source(); // fresh list for each new search, the pattern changes in between
    }

    QVector<QPair<int, int>> matches; // score, item index
    for (int i = 0; i < m_items.size(); ++i)
    {
        const int score = fuzzyScore(text, m_items.at(i).name);
        if (score >= 0)
        {
            matches.append(qMakePair(score, i));
        }
    }
    const int count = qMin(matches.size(), max_results);
    std::partial_sort(matches.begin(), matches.begin() + count, matches.end(),
                      [this](const QPair<int, int> &a, const QPair<int, int> &b)
    {
        return a.first != b.first ? a.first > b.first
                                  : m_items.at(a.second).name.size() < m_items.at(b.second).name.size();
    });

    // A new model per search resets the view in one step. Removing and inserting rows while an accessibility
    // client (VoiceOver) is attached crashes Qt 6.12 in QAccessibleCache::deleteInterface.
    QStandardItemModel *model = new QStandardItemModel(0, 2, this);
    for (int i = 0; i < count; ++i)
    {
        const SearchItem &item = m_items.at(matches.at(i).second);
        QStandardItem *name = new QStandardItem(item.icon, item.name);
        name->setData(matches.at(i).second, Qt::UserRole);
        QStandardItem *detail = new QStandardItem(item.detail);
        detail->setForeground(ui->search_line_edit->palette().placeholderText());
        model->appendRow({name, detail});
    }

    QStandardItem *help = new QStandardItem(QIcon(":/icon/32x32/find.png"),
                                            tr("Search Seamly help for \"%1\"").arg(text.trimmed()));
    help->setData(-1, Qt::UserRole);
    model->appendRow({help, new QStandardItem});

    m_completer->setModel(model);
    m_model->deleteLater();
    m_model = model;
    QHeaderView *header = static_cast<QTreeView *>(m_completer->popup())->header();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    const QRect rect = ui->search_line_edit->rect();
    m_completer->complete(QRect(rect.left(), rect.top(), qMax(rect.width(), bar_width), rect.height()));
    m_completer->popup()->setCurrentIndex(m_model->index(0, 0));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief run runs the chosen item, or opens the help search for the typed text, and clears the field.
/// @param item index in m_items, -1 for the help search.
void SearchBar::run(int item)
{
    const QString text = ui->search_line_edit->text().trimmed();
    ui->search_line_edit->clear();
    ui->search_line_edit->clearFocus();

    if (item < 0)
    {
        QUrl url(QStringLiteral("https://wiki.seamly.io/wiki/Special:Search"));
        url.setQuery(QUrlQuery({{QStringLiteral("search"), text}}));
        QDesktopServices::openUrl(url);
    }
    else if (item < m_items.size() && m_items.at(item).run)
    {
        m_items.at(item).run();
    }
}

#if defined(Q_OS_MACOS)
//---------------------------------------------------------------------------------------------------------------------
/// @brief placeInTitleBar hides the window title and centers the field in the title bar, which the window content
/// extends under. Hidden when there is no title bar, as in full screen.
void SearchBar::placeInTitleBar()
{
    hideTitleText(m_window);
    const int title_height = m_window->windowHandle() ? m_window->windowHandle()->safeAreaMargins().top() : 0;
    const int width = qMin(bar_width, m_window->width() / 2);
    setGeometry((m_window->width() - width) / 2, (title_height - bar_height) / 2, width, bar_height);
    setVisible(title_height >= bar_height);
    raise();
}
#endif
