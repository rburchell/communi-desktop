/*
* Copyright (C) 2008-2013 The Communi Project
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

#ifndef MAINWINDOWPLUGIN_H
#define MAINWINDOWPLUGIN_H

#include <QtPlugin>

class MainWindow;

class MainWindowPlugin
{
public:
    virtual ~MainWindowPlugin() {}
    virtual void initWindow(MainWindow*) {}
    virtual void cleanupWindow(MainWindow*) {}
};

Q_DECLARE_INTERFACE(MainWindowPlugin, "Communi.MainWindowPlugin")

#endif // MAINWINDOWPLUGIN_H
