/*
 * Copyright 2017, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

/* GTK 4 has a public GtkEmojiChooser, so the copy GTK 3 needed is gone
   and this only hands out the GTK one (a GtkPopover, "emoji-picked"). */

#include "e-gtkemojichooser.h"

GType
e_gtk_emoji_chooser_get_type (void)
{
	return GTK_TYPE_EMOJI_CHOOSER;
}

GtkWidget *
e_gtk_emoji_chooser_new (void)
{
	return gtk_emoji_chooser_new ();
}
