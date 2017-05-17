/*
 * This file is part of mt7697
 *
 * Copyright (C) 2017 Sierra Wireless Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __MT7697_BIT_OPS_H__
#define __MT7697_BIT_OPS_H__

// Clears a bitfield from _value_
#define BF_CLEAR(v, o, w) ((v) & ~GENMASK((o) + (w), (o)))

/* Extracts a bitfield from a value */
#define BF_GET(v, o, w) (((v) >> (o)) & GENMASK((w), 0))

/* Compose a value with the given bitfield set to v */
#define BF_DEFINE(v, o, w) (((v) & GENMASK((w), 0)) << (o))

/* Replace the given bitfield with the value in b */
#define BF_SET(v, b, o, w) (BF_CLEAR(v, o, w) | BF_DEFINE(b, o, w))

#endif
