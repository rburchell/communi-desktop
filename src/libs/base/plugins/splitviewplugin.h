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

#ifndef SPLITVIEWPLUGIN_H
#define SPLITVIEWPLUGIN_H

#include <QtPlugin>

class SplitView;

class SplitViewPlugin
{
public:
    virtual ~SplitViewPlugin() {}
    virtual void initView(SplitView*) {}
    virtual void cleanupView(SplitView*) {}
};

Q_DECLARE_INTERFACE(SplitViewPlugin, "Communi.SplitViewPlugin")

#endif // SPLITVIEWPLUGIN_H
