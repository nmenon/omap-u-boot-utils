/*
 * liblcfg - lightweight configuration file library
 * Copyright (c) 2007--2009  Paul Baecher
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *    $Id: lcfg.h 1446 2009-01-15 22:44:36Z dp $
 *
 */

#ifndef LCFG_H
#define LCFG_H

#include <stdlib.h>

struct lcfg;

enum lcfg_status { lcfg_status_ok, lcfg_status_error };

typedef enum lcfg_status (*lcfg_visitor_function)(const char *key, void *data, size_t size, void *user_data);


/* open a new config file */
struct lcfg *        lcfg_new(const char *filename);

/* parse config into memory */
enum lcfg_status     lcfg_parse(struct lcfg *);

/* visit all configuration elements */
enum lcfg_status     lcfg_accept(struct lcfg *, lcfg_visitor_function, void *);

/* access a value by path */
enum lcfg_status     lcfg_value_get(struct lcfg *, const char *, void **, size_t *);

/* return the last error message */
const char *         lcfg_error_get(struct lcfg *);

/* set error */
void                 lcfg_error_set(struct lcfg *, const char *fmt, ...);

/* destroy lcfg context */
void                 lcfg_delete(struct lcfg *);


#endif
/*
 * liblcfg - lightweight configuration file library
 * Copyright (c) 2007--2009  Paul Baecher
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *    $Id$
 *
 */

#ifndef LCFGX_TREE_H
#define LCFGX_TREE_H


enum lcfgx_type
{
	lcfgx_string,
	lcfgx_list,
	lcfgx_map,
};

struct lcfgx_tree_node
{
	enum lcfgx_type type;
	char *key; /* NULL for root node */

	union
	{
		struct
		{
			void *data;
			size_t len;
		} string;
		struct lcfgx_tree_node *elements; /* in case of list or map type */
	} value;

	struct lcfgx_tree_node *next;
};

struct lcfgx_tree_node *lcfgx_tree_new(struct lcfg *);

void lcfgx_tree_delete(struct lcfgx_tree_node *);


#endif

