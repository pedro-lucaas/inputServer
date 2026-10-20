/*
 * InputServer -- mouse and keyboard sharing utility
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

class QApplication;
class QWidget;

namespace theme {

// Applies the InputServer look (Fusion style, palette and stylesheet) and
// keeps it in sync with the system light/dark setting.
void install(QApplication& app);

// Re-evaluates stylesheet rules that depend on dynamic properties, e.g.
// after changing the "state" property of the status pill.
void repolish(QWidget* widget);

} // namespace theme
