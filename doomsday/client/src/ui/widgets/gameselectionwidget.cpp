/** @file gameselectionwidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/gameselectionwidget.h"
#include "CommandAction"
#include "clientapp.h"
#include "games.h"
#include "dd_main.h"

#include <de/ui/ActionItem>
#include <de/MenuWidget>
#include <de/SequentialLayout>
#include <de/FoldPanelWidget>
#include <de/DocumentPopupWidget>
#include <de/SignalAction>
#include <de/FIFO>
#include <QMap>

#include "CommandAction"

using namespace de;

DENG_GUI_PIMPL(GameSelectionWidget)
, DENG2_OBSERVES(Games, Addition)
, DENG2_OBSERVES(App, StartupComplete)
, public ChildWidgetOrganizer::IWidgetFactory
{
    /// ActionItem with a Game member, for loading a particular game.
    struct GameItem : public ui::ActionItem {
        GameItem(Game const &gameRef, de::String const &label, de::Action *action)
            : ui::ActionItem(label, action), game(gameRef) {
            setData(&gameRef);
        }
        Game const &game;
    };

    /**
     * Widget for representing an item in the game selection menu. Has two buttons:
     * one for starting the game and one for configuring it.
     */
    struct GameWidget
            : public GuiWidget
            , DENG2_OBSERVES(ButtonWidget, Press)
            , DENG2_OBSERVES(App, GameUnload)
    {
        ButtonWidget *load;
        ButtonWidget *info;
        DocumentPopupWidget *popup;
        Game const *game;

        GameWidget() : game(0)
        {
            rule().setInput(Rule::Height, style().fonts().font("default").height() * 4);

            add(load = new ButtonWidget);
            add(info = new ButtonWidget);

            load->disable();
            load->setBehavior(Widget::ContentClipping);
            load->setAlignment(ui::AlignLeft);
            load->setTextAlignment(ui::AlignRight);
            load->setTextLineAlignment(ui::AlignLeft);

            info->setWidthPolicy(ui::Expand);
            info->setAlignment(ui::AlignBottom);
            info->setText(_E(s)_E(B) + tr("..."));

            add(popup = new DocumentPopupWidget);
            popup->setAnchorAndOpeningDirection(info->rule(), ui::Up);
            popup->document().setMaximumLineWidth(popup->style().rules().rule("document.popup.width").valuei());
            info->audienceForPress += this;

            // Button for extra information.
            load->rule()
                    .setInput(Rule::Left,   rule().left())
                    .setInput(Rule::Top,    rule().top())
                    .setInput(Rule::Bottom, rule().bottom())
                    .setInput(Rule::Right,  info->rule().left());
            info->rule()
                    .setInput(Rule::Top,    rule().top())
                    .setInput(Rule::Right,  rule().right())
                    .setInput(Rule::Bottom, rule().bottom());

            App::app().audienceForGameUnload += this;
        }

        ~GameWidget()
        {
            App::app().audienceForGameUnload -= this;
        }

        void aboutToUnloadGame(game::Game const &)
        {
            popup->close(0);
        }

        void buttonPressed(ButtonWidget &bt)
        {
            /*
            // Show information about the game.
            popup->setAnchorAndOpeningDirection(
                        bt.rule(),
                        bt.rule().top().valuei() + bt.rule().height().valuei() / 2 <
                        bt.root().viewRule().height().valuei() / 2?
                            ui::Down : ui::Up);*/
            popup->document().setText(game->description());
            popup->open();
        }
    };

    struct Subset : public FoldPanelWidget
    {
        MenuWidget *menu;

        Subset(String const &headingText, GameSelectionWidget::Instance *owner)
        {
            owner->self.add(makeTitle(headingText));
            title().setFont("title");
            title().setTextColor("inverted.text");
            title().setAlignment(ui::AlignLeft);
            title().margins().setLeft("");

            setContent(menu = new MenuWidget);
            menu->margins().set("");
            menu->layout().setColumnPadding(owner->style().rules().rule("unit"));
            menu->enableScrolling(false);
            menu->organizer().setWidgetFactory(*owner);

            menu->rule().setInput(Rule::Width,
                                  owner->self.rule().width() -
                                  owner->self.margins().width());

            setColumns(3);
        }

        void setColumns(int cols)
        {
            if(menu->layout().maxGridSize().x != cols)
            {
                menu->setGridSize(cols, ui::Filled, 0, ui::Expand);
            }
        }

        ui::Data &items()
        {
            return menu->items();
        }

        GameWidget &gameWidget(ui::Item const &item)
        {
            return menu->itemWidget<GameWidget>(item);
        }
    };

    FIFO<Game> pendingGames;
    SequentialLayout superLayout;

    Subset *available;
    Subset *incomplete;

    Instance(Public *i)
        : Base(i)
        , superLayout(i->contentRule().left(), i->contentRule().top(), ui::Down)
    {
        // Menu of available games.
        self.add(available = new Subset(tr("Available Games"), this));

        // Menu of incomplete games.
        self.add(incomplete = new Subset(tr("Incomplete Games"), this));

        superLayout.setOverrideWidth(self.rule().width() - self.margins().width());

        available->title().margins().setTop("");

        superLayout << available->title()
                    << *available
                    << incomplete->title()
                    << *incomplete;

        self.setContentSize(superLayout.width(), superLayout.height());

        App_Games().audienceForAddition += this;
        App::app().audienceForStartupComplete += this;
    }

    ~Instance()
    {
        App_Games().audienceForAddition -= this;
        App::app().audienceForStartupComplete -= this;
    }

    void gameAdded(Game &game)
    {
        // Called from a non-UI thread.
        pendingGames.put(&game);
    }

    void addPendingGames()
    {
        while(Game *game = pendingGames.take())
        {
            incomplete->items().append(makeItemForGame(*game));
        }
    }

    ui::Item *makeItemForGame(Game &game)
    {
        String const idKey = game.identityKey();

        CommandAction *loadAction = new CommandAction(String("load ") + idKey);
        String label = String(_E(b) "%1" _E(.) /*_E(s)_E(C) " %2\n" _E(.)_E(.)*/ "\n"
                              _E(l)_E(D) "%2")
                .arg(game.title())
                //.arg(game.author())
                .arg(idKey);

        GameItem *item = new GameItem(game, label, loadAction);

        /// @todo The name of the plugin should be accessible via the plugin loader.
        String plugName;
        if(idKey.contains("heretic"))
        {
            plugName = "libheretic";
        }
        else if(idKey.contains("hexen"))
        {
            plugName = "libhexen";
        }
        else
        {
            plugName = "libdoom";
        }
        if(style().images().has(game.logoImageId()))
        {
            item->setImage(style().images().image(game.logoImageId()));
        }

        return item;
    }

    GuiWidget *makeItemWidget(ui::Item const &, GuiWidget const *)
    {
        return new GameWidget;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item)
    {
        GameWidget &w = widget.as<GameWidget>();
        GameItem const &it = item.as<GameItem>();

        w.game = &it.game;

        w.load->setImage(it.image());
        w.load->setText(it.label());
        w.load->setAction(it.action()->duplicate());
    }

    void appStartupCompleted()
    {
        updateGameAvailability();
    }

    void updateGameAvailability()
    {
        // Available games.
        for(uint i = 0; i < available->items().size(); ++i)
        {
            GameItem const &item = available->items().at(i).as<GameItem>();
            if(item.game.allStartupFilesFound())
            {
                available->gameWidget(item).load->enable();
            }
            else
            {
                incomplete->items().append(available->items().take(i--));
                incomplete->gameWidget(item).load->disable();
            }
        }

        // Incomplete games.
        for(uint i = 0; i < incomplete->items().size(); ++i)
        {
            GameItem const &item = incomplete->items().at(i).as<GameItem>();
            if(item.game.allStartupFilesFound())
            {
                available->items().append(incomplete->items().take(i--));
                available->gameWidget(item).load->enable();
            }
            else
            {
                incomplete->gameWidget(item).load->disable();
            }
        }

        available->items().sort();
        incomplete->items().sort();
    }
};

GameSelectionWidget::GameSelectionWidget(String const &name)
    : ScrollAreaWidget(name), d(new Instance(this))
{
    d->available->open();
    d->incomplete->open();

    // We want the full menu to be visible even when it doesn't fit the
    // designated area.
    unsetBehavior(ChildVisibilityClipping);
}

void GameSelectionWidget::viewResized()
{
    ScrollAreaWidget::viewResized();

    // If the view is too small, we'll want to reduce the number of items in the menu.
    int const maxWidth  = style().rules().rule("gameselection.max.width").valuei();
    int const maxHeight = style().rules().rule("gameselection.max.height").valuei();

    Vector2i suitable(clamp(1, 3 * rule().width().valuei() / maxWidth,   3),
                      clamp(1, 8 * rule().height().valuei() / maxHeight, 6));

    d->available->setColumns(suitable.x);
    d->incomplete->setColumns(suitable.x);
}

void GameSelectionWidget::update()
{
    d->addPendingGames();

    ScrollAreaWidget::update();
}
