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

#include "../vmisc/vabstractapplication.h"
#include "../vmisc/vcommonsettings.h"

#if defined(Q_OS_MACOS)
#include "../core/macos_dock.h"
#endif

#include <QApplication>
#include <QCompleter>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QListView>
#include <QMainWindow>
#include <QMenuBar>
#include <QPainter>
#include <QShortcut>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QUrlQuery>
#include <QWindow>

#include <algorithm>

namespace
{
const int max_results     = 50;
const int max_suggestions = 8;
const int max_recent      = 5;
const int bar_height      = 24;
const int bar_width       = 420;
const int popup_width     = 560;
const int row_spacing     = 3;
const int row_height      = 32;
const int max_rows        = 12;

const QString recent_key = QStringLiteral("searchBar/recent");

enum Role
{
    ItemRole = Qt::UserRole,
    KindRole,
    ShortcutRole,
    DetailRole
};

//---------------------------------------------------------------------------------------------------------------------
/// @brief withAlpha returns color with its alpha set to alpha.
QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

class SearchItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    //-----------------------------------------------------------------------------------------------------------------
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(0, row_height);
    }

    //-----------------------------------------------------------------------------------------------------------------
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const int padding = 10;
        const int cap_height = 20;
        const QRect row = option.rect.adjusted(row_spacing, 0, -row_spacing, 0);
        const bool selected = option.state & QStyle::State_Selected;
        // The application palette, the style sheet replaces the view's selection color with a gray.
        const QPalette palette = QApplication::palette();
        const QColor text = palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        const QColor muted = withAlpha(text, 0.6);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (selected || option.state & QStyle::State_MouseOver)
        {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? palette.color(QPalette::Highlight) : withAlpha(text, 0.08));
            painter->drawRoundedRect(row, 4, 4);
        }

        int right = row.right() - padding;
        const QStringList caps = SearchBar::keyCaps(index.data(ShortcutRole).toString());
        QFont cap_font = option.font;
        cap_font.setPointSizeF(cap_font.pointSizeF() * 0.9);
        const QFontMetrics cap_metrics(cap_font);
        painter->setFont(cap_font);
        for (int i = caps.size() - 1; i >= 0; --i)
        {
            const int width = qMax(cap_height, cap_metrics.horizontalAdvance(caps.at(i)) + 10);
            const QRect cap(right - width + 1, row.center().y() - cap_height / 2 + 1, width, cap_height);
            painter->setPen(withAlpha(text, 0.25));
            painter->setBrush(withAlpha(text, 0.1));
            painter->drawRoundedRect(QRectF(cap).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
            painter->setPen(text);
            painter->drawText(cap, Qt::AlignCenter, caps.at(i));
            right = cap.left() - 4;
        }
        painter->setFont(option.font);

        const QString detail = index.data(DetailRole).toString();
        if (!detail.isEmpty())
        {
            painter->setPen(muted);
            const int width = option.fontMetrics.horizontalAdvance(detail);
            painter->drawText(QRect(right - width, row.top(), width, row.height()), Qt::AlignVCenter, detail);
            right -= width + padding;
        }

        int left = row.left() + padding;
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        icon.paint(painter, QRect(left, row.center().y() - 8 + 1, 16, 16));
        left += 16 + 8;

        const QString kind = index.data(KindRole).toString();
        const int kind_width = kind.isEmpty() ? 0 : option.fontMetrics.horizontalAdvance(kind) + 8;
        const QString name = option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                                           qMax(0, right - left - kind_width));
        const int name_width = option.fontMetrics.horizontalAdvance(name);
        painter->setPen(text);
        painter->drawText(QRect(left, row.top(), name_width, row.height()), Qt::AlignVCenter, name);
        if (!kind.isEmpty())
        {
            painter->setPen(muted);
            painter->drawText(QRect(left + name_width + 8, row.top(), kind_width, row.height()), Qt::AlignVCenter,
                              kind);
        }
        painter->restore();
    }
};

class SearchPopup : public QListView
{
public:
    using QListView::QListView;

protected:
    //-----------------------------------------------------------------------------------------------------------------
    virtual void paintEvent(QPaintEvent *event) override
    {
        // The window is transparent, the rounded background is painted here instead of by the style sheet.
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        const QPalette palette = QApplication::palette();
        painter.setPen(withAlpha(palette.color(QPalette::Text), 0.2));
        painter.setBrush(palette.color(QPalette::Window));
        painter.drawRoundedRect(QRectF(viewport()->rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        painter.end();
        QListView::paintEvent(event);
    }

    //-----------------------------------------------------------------------------------------------------------------
    virtual void scrollContentsBy(int dx, int dy) override
    {
        QListView::scrollContentsBy(dx, dy);
        viewport()->update(); // a scrolled copy would move the rounded corners
    }
};
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
    , m_model(new QStandardItemModel(this))
    , m_source()
    , m_items()
    , m_recent(qApp->Settings()->value(recent_key).toStringList())
{
    ui->setupUi(this);
    ui->search_line_edit->addAction(QIcon(":/icon/32x32/find.png"), QLineEdit::LeadingPosition);
    ui->search_line_edit->installEventFilter(this);

    // The themes leave the system placeholder color, which is unreadable when the system and theme brightness differ.
    QPalette palette = ui->search_line_edit->palette();
    palette.setColor(QPalette::PlaceholderText, withAlpha(palette.color(QPalette::Text), 0.5));
    ui->search_line_edit->setPalette(palette);

    // Not a QTreeView: its accessibility update crashes Qt 6.12.0 when the results change (QTBUG-149612).
    QListView *popup = new SearchPopup;
    popup->setItemDelegate(new SearchItemDelegate(popup));
    popup->setUniformItemSizes(true);
    popup->setSpacing(row_spacing);
    popup->setMouseTracking(true);
    popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    popup->setStyleSheet(QStringLiteral("QListView { background: transparent; border: none; padding: 0; }"));

    m_completer->setWidget(ui->search_line_edit);
    m_completer->setPopup(popup);
    m_completer->setModel(m_model);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setMaxVisibleItems(max_rows);

    popup->setAttribute(Qt::WA_TranslucentBackground);
    popup->viewport()->setAutoFillBackground(false);

    connect(ui->search_line_edit, &QLineEdit::textChanged, this, &SearchBar::search);
    connect(m_completer, QOverload<const QModelIndex &>::of(&QCompleter::activated), this,
            [this](const QModelIndex &index)
    {
        run(index.data(ItemRole).toInt());
    });

    QShortcut *shortcut = new QShortcut(QKeySequence::Find, window);
    connect(shortcut, &QShortcut::activated, this, [this]()
    {
        ui->search_line_edit->setFocus(Qt::ShortcutFocusReason);
        ui->search_line_edit->selectAll();
        search(ui->search_line_edit->text());
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
/// @brief keyCaps splits a shortcut in native text into the keys to draw, modifiers apart: "⇧⌘S" or "Ctrl+Shift+S"
/// gives three keys. Chords are separated by ", ".
/// @param shortcut shortcut as QKeySequence::NativeText.
/// @return keys in order.
QStringList SearchBar::keyCaps(const QString &shortcut)
{
    QStringList caps;
    const QStringList chords = shortcut.split(QStringLiteral(", "), Qt::SkipEmptyParts);
    for (const QString &chord : chords)
    {
        int i = 0;
        while (i < chord.size() - 1 && QStringLiteral("⌃⌥⇧⌘").contains(chord.at(i)))
        {
            caps.append(chord.at(i++));
        }
        const QString keys = chord.mid(i);
        if (keys == QLatin1String("+"))
        {
            caps.append(keys);
        }
        else if (keys.endsWith(QLatin1String("++")))
        {
            caps.append(keys.chopped(2).split(QLatin1Char('+')));
            caps.append(QStringLiteral("+"));
        }
        else
        {
            caps.append(keys.split(QLatin1Char('+')));
        }
    }
    return caps;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief eventFilter keeps the field in the title bar, opens the suggestions on a click, and handles Escape and Down
/// in the field.
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
        if (event->type() == QEvent::MouseButtonRelease && !m_completer->popup()->isVisible())
        {
            search(ui->search_line_edit->text());
        }
        else if (event->type() == QEvent::KeyPress)
        {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Escape)
            {
                ui->search_line_edit->clearFocus();
                ui->search_line_edit->clear();
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
/// @brief search scores every item against text and shows the best ones, best first, plus a help search entry. With
/// no text it shows the recently run items, then the suggested ones.
/// @param text what the user typed.
void SearchBar::search(const QString &text)
{
    if (!ui->search_line_edit->hasFocus())
    {
        m_completer->popup()->hide();
        return;
    }
    if (!m_completer->popup()->isVisible() && m_source)
    {
        m_items = m_source(); // fresh list for each new search, the pattern changes in between
    }

    QVector<int> rows;
    if (text.trimmed().isEmpty())
    {
        for (const QString &name : qAsConst(m_recent))
        {
            const auto item = std::find_if(m_items.cbegin(), m_items.cend(),
                                           [&name](const SearchItem &item) { return item.name == name; });
            if (item != m_items.cend())
            {
                rows.append(static_cast<int>(item - m_items.cbegin()));
            }
        }
        for (int i = 0; i < m_items.size() && rows.size() < max_suggestions; ++i)
        {
            if (m_items.at(i).suggested && !rows.contains(i))
            {
                rows.append(i);
            }
        }
        showRows(rows, QString());
        return;
    }

    QVector<QPair<int, int>> matches; // score, item index
    for (int i = 0; i < m_items.size(); ++i)
    {
        const SearchItem &item = m_items.at(i);
        int score = fuzzyScore(text, item.name);
        if (!item.kind.isEmpty())
        {
            score = qMax(score, fuzzyScore(text, item.name + QLatin1Char(' ') + item.kind) - 5);
        }
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
    for (int i = 0; i < count; ++i)
    {
        rows.append(matches.at(i).second);
    }
    showRows(rows, text.trimmed());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief showRows shows items in the popup, plus a help search entry for text if it isn't empty.
/// @param rows indexes in m_items.
/// @param text what the user typed.
void SearchBar::showRows(const QVector<int> &rows, const QString &text)
{
    // A new model per search resets the view in one step.
    QStandardItemModel *model = new QStandardItemModel(this);
    for (int row : rows)
    {
        const SearchItem &item = m_items.at(row);
        QStandardItem *name = new QStandardItem(item.icon, item.name);
        name->setData(row, ItemRole);
        name->setData(item.kind, KindRole);
        name->setData(item.shortcut, ShortcutRole);
        name->setData(item.detail, DetailRole);
        model->appendRow(name);
    }
    if (!text.isEmpty())
    {
        QStandardItem *help = new QStandardItem(QIcon(":/icon/32x32/find.png"),
                                                tr("Search Seamly help for \"%1\"").arg(text));
        help->setData(-1, ItemRole);
        model->appendRow(help);
    }

    m_completer->setModel(model);
    m_model->deleteLater();
    m_model = model;
    if (model->rowCount() == 0)
    {
        m_completer->popup()->hide();
        return;
    }

    QAbstractItemView *popup = m_completer->popup();
    const QRect rect = ui->search_line_edit->rect();
    const int visible_rows = qMin(model->rowCount(), max_rows);
    m_completer->complete(QRect((rect.width() - popup_width) / 2, rect.top(), popup_width, rect.height() + 4));
    popup->resize(popup_width, visible_rows * (row_height + row_spacing) + row_spacing);
    popup->setCurrentIndex(model->index(0, 0));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief run runs the chosen item, or opens the help search for the typed text, and clears the field.
/// @param item index in m_items, -1 for the help search.
void SearchBar::run(int item)
{
    const QString text = ui->search_line_edit->text().trimmed();
    ui->search_line_edit->clearFocus();
    ui->search_line_edit->clear();

    if (item < 0)
    {
        QUrl url(QStringLiteral("https://wiki.seamly.io/wiki/Special:Search"));
        url.setQuery(QUrlQuery({{QStringLiteral("search"), text}}));
        QDesktopServices::openUrl(url);
    }
    else if (item < m_items.size())
    {
        m_recent.removeAll(m_items.at(item).name);
        m_recent.prepend(m_items.at(item).name);
        m_recent = m_recent.mid(0, max_recent);
        qApp->Settings()->setValue(recent_key, m_recent);
        if (m_items.at(item).run)
        {
            m_items.at(item).run();
        }
    }
}

#if defined(Q_OS_MACOS)
//---------------------------------------------------------------------------------------------------------------------
/// @brief placeInTitleBar hides the window title and centers the field in the title bar, which the window content
/// extends under. Full screen has no title bar, so the window keeps a band of the same height at the top instead.
void SearchBar::placeInTitleBar()
{
    hideTitleText(m_window);
    const int title_height = m_window->windowHandle() ? m_window->windowHandle()->safeAreaMargins().top() : 0;
    const int band_height = title_height >= bar_height ? 0 : bar_height + 8;
    m_window->setContentsMargins(0, band_height, 0, 0);

    const int width = qMin(bar_width, m_window->width() / 2);
    setGeometry((m_window->width() - width) / 2, (qMax(title_height, band_height) - bar_height) / 2, width,
                bar_height);
    raise();
}
#endif
