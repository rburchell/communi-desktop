/*
* Copyright (C) 2008-2014 The Communi Project
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#ifndef TEXTINPUT_H
#define TEXTINPUT_H

#include "historylineedit.h"
class IrcCompleter;

class TextInput : public HistoryLineEdit
{
    Q_OBJECT
    Q_PROPERTY(int lag READ lag WRITE setLag)

public:
    TextInput(QWidget* parent = 0);

    IrcCompleter* completer() const;

    int lag() const;

public slots:
    void setLag(int lag);

signals:
    void send(const QString& text);
    void typed(const QString& text);
    void scrollToTop();
    void scrollToBottom();
    void scrollToNextPage();
    void scrollToPreviousPage();

protected:
    bool event(QEvent* event);
    void paintEvent(QPaintEvent* event);

private slots:
    void onSend();
    void tryComplete();
    void onCompleted(const QString& text, int cursor);

private:
    struct Private {
        int lag;
        IrcCompleter* completer;
    } d;
};

#endif // TEXTINPUT_H