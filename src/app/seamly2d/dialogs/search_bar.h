//-----------------------------------------------------------------------------
//  @file   search_bar.h
//  @author palepal
//  @date   1 Nov, 2026
//
//  @copyright
//  Copyright (C) 2026 palepal
//  https://github.com/palepal/Seamly2D
//
//  @brief
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see <http://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------

#ifndef SEARCH_BAR_H
#define SEARCH_BAR_H

#include <QIcon>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QCompleter;
class QMainWindow;
class QStandardItemModel;

namespace Ui
{
    class SearchBar;
}

struct SearchItem
{
    QString               name;
    QString               kind;      // shown after the name and searched with it, e.g. "Tool"
    QString               shortcut;  // native text, drawn as key caps
    QString               detail;    // right aligned value
    QIcon                 icon;
    std::function<void()> run;
    bool                  suggested = false; // listed while the field is empty
};

class SearchBar : public QWidget
{
    Q_OBJECT
public:
    explicit SearchBar(QMainWindow *window);
    virtual ~SearchBar() override;

    void               setFileName(const QString &name);
    void               setItemSource(const std::function<QVector<SearchItem>()> &source);
    static int         fuzzyScore(const QString &pattern, const QString &text);
    static QStringList keyCaps(const QString &shortcut);

protected:
    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    Q_DISABLE_COPY(SearchBar)
    Ui::SearchBar                        *ui;
    QMainWindow                          *m_window;
    QCompleter                           *m_completer;
    QStandardItemModel                   *m_model;
    std::function<QVector<SearchItem>()>  m_source;
    QVector<SearchItem>                   m_items;
    QStringList                           m_recent;

    void search(const QString &text);
    void showRows(const QVector<int> &rows, const QString &text, const QVector<int> &recent = QVector<int>());
    void run(int item);
#if defined(Q_OS_MACOS)
    void placeInTitleBar();
#endif
};

#endif // SEARCH_BAR_H
