/*
obs-whitebalance
Copyright (C) 2026 obs-whitebalance contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#ifdef __cplusplus
#include <QColor>
#include <QIcon>

/* Eyedropper icon drawn in the given color (1x and 2x pixmaps). */
QIcon wb_eyedropper_icon(const QColor &color, int size);

extern "C" {
#endif

/* Install/remove the properties-view cosmetic tweaks (UI thread). */
void wb_ui_install(void);
void wb_ui_uninstall(void);

#ifdef __cplusplus
}
#endif
