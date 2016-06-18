/** @file uisettingsdialog.cpp  User interface settings.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/dialogs/uisettingsdialog.h"
#include "clientapp.h"

#include <de/GridLayout>
#include <de/CallbackAction>
#include <de/VariableChoiceWidget>
#include <de/VariableToggleWidget>
#include <de/App>

using namespace de;

DENG2_PIMPL(UISettingsDialog)
{
    VariableChoiceWidget *uiScale;
    VariableToggleWidget *showAnnotations;
    VariableToggleWidget *showColumnDescription;
    VariableToggleWidget *showUnplayable;
    VariableToggleWidget *showDoom;
    VariableToggleWidget *showHeretic;
    VariableToggleWidget *showHexen;
    VariableToggleWidget *showOther;
    VariableToggleWidget *showMultiplayer;

    Instance(Public *i) : Base(i)
    {
        auto &area = self.area();

        area.add(uiScale               = new VariableChoiceWidget(App::config("ui.scaleFactor")));
        area.add(showAnnotations       = new VariableToggleWidget(tr("Menu Annotations"), App::config("ui.showAnnotations")));

        area.add(showColumnDescription = new VariableToggleWidget(tr("Game Descriptions"), App::config("home.showColumnDescription")));
        area.add(showUnplayable        = new VariableToggleWidget(tr("Show Unplayable Games"), App::config("home.showUnplayableGames")));
        area.add(showDoom              = new VariableToggleWidget(tr("Show Doom"), App::config("home.columns.doom")));
        area.add(showHeretic           = new VariableToggleWidget(tr("Show Heretic"), App::config("home.columns.heretic")));
        area.add(showHexen             = new VariableToggleWidget(tr("Show Hexen"), App::config("home.columns.hexen")));
        area.add(showOther             = new VariableToggleWidget(tr("Show Other Games"), App::config("home.columns.otherGames")));
        area.add(showMultiplayer       = new VariableToggleWidget(tr("Show Multiplayer"), App::config("home.columns.multiplayer")));

        uiScale->items()
                << new ChoiceItem(tr("Huge (200%)"),   2.0)
                << new ChoiceItem(tr("Large (150%)"),  1.5)
                << new ChoiceItem(tr("Normal (100%)"), 1.0)
                << new ChoiceItem(tr("Small (90%)"),   0.9)
                << new ChoiceItem(tr("Tiny (80%)"),    0.8);
        uiScale->updateFromVariable();
    }

    void resetToDefaults()
    {
        ClientApp::uiSettings().resetToDefaults();
    }
};

UISettingsDialog::UISettingsDialog(String const &name)
    : DialogWidget(name, WithHeading)
    , d(new Instance(this))
{
    heading().setText(tr("UI Settings"));
    heading().setImage(style().images().image("home.icon"));

    auto *library = LabelWidget::newWithText(_E(D) + tr("Game Library"), &area());
    library->setFont("separator.label");

    auto *restartNotice = LabelWidget::newWithText(tr("Takes effect only after restarting."), &area());
    restartNotice->margins().setTop("");
    restartNotice->setFont("separator.annotation");
    restartNotice->setTextColor("altaccent");

    d->showAnnotations->margins().setBottom("unit");

    auto *annots = LabelWidget::newWithText(tr("Annotations briefly describe menu functions."), &area());
    annots->margins().setTop("");
    annots->setFont("separator.annotation");
    annots->setTextColor("altaccent");

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *LabelWidget::newWithText(tr("Scale:"), &area()) << *d->uiScale
           << Const(0) << *restartNotice
           << Const(0) << *d->showAnnotations
           << Const(0) << *annots;
    layout.setCellAlignment(Vector2i(0, layout.gridSize().y), ui::AlignLeft);
    layout.append(*library, 2);
    layout << Const(0) << *d->showColumnDescription
           << Const(0) << *d->showUnplayable
           << Const(0) << *d->showDoom
           << Const(0) << *d->showHeretic
           << Const(0) << *d->showHexen
           << Const(0) << *d->showOther
           << Const(0) << *d->showMultiplayer;

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new CallbackAction([this] () { d->resetToDefaults(); }));
}