# -*- coding: utf-8 -*-
# vim:set et sts=4 sw=4:
#
# libzhuyin - Library to deal with zhuyin.
#
# Copyright (C) 2013 Peng Wu <alexepico@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

# for Hsu and ET26

hsu_correct = [
    # "correct", "wrong"
    ("ㄓ" , "ㄐ"),
    ("ㄔ" , "ㄑ"),
    ("ㄕ" , "ㄒ"),
    ("ㄛ" , "ㄏ"),
    ("ㄜ" , "ㄍ"),
    ("ㄢ" , "ㄇ"),
    ("ㄣ" , "ㄋ"),
    ("ㄤ" , "ㄎ"),
    ("ㄦ" , "ㄌ"),
    ("ㄐㄧ*" , "ㄍㄧ*"),
    ("ㄐㄩ*" , "ㄍㄩ*"),
    ("ㄓㄨ*" , "ㄐㄨ*"),
    ("ㄔㄨ*" , "ㄑㄨ*"),
    ("ㄕㄨ*" , "ㄒㄨ*"),
#    ("ㄐㄧ*" , "ㄍㄧ*"),
#    ("ㄐㄩ*" , "ㄍㄩ*"),
]


hsu_correct_special = [
# "correct", "wrong"
# ㄐㄑㄒ must follow ㄧㄩ
# m_middle == zero from libchewing code
    ("ㄓ*" , "ㄐ*"),
    ("ㄔ*" , "ㄑ*"),
    ("ㄕ*" , "ㄒ*"),
]


et26_correct = [
    # "correct", "wrong"
    ("ㄓ" , "ㄐ"),
    ("ㄕ" , "ㄒ"),
    ("ㄡ" , "ㄆ"),
    ("ㄢ" , "ㄇ"),
    ("ㄣ" , "ㄋ"),
    ("ㄤ" , "ㄊ"),
    ("ㄥ" , "ㄌ"),
    ("ㄦ" , "ㄏ"),
    ("ㄓㄨ*" , "ㄐㄨ*"),
    ("ㄕㄨ*" , "ㄒㄨ*"),
    ("ㄑㄧ*" , "ㄍㄧ*"),
    ("ㄑㄩ*" , "ㄍㄩ*"),
]


et26_correct_special = [
# "correct", "wrong"
# ㄐㄒ must follow ㄧㄩ
# m_middle == zero from libchewing code
    ("ㄓ*" , "ㄐ*"),
    ("ㄕ*" , "ㄒ*"),
]


dachen_cp26_switch = [
# switch key, from, to
    ('q', "ㄅ", "ㄆ"),
    ('q', "ㄆ", "ㄅ"),

    ('w', "ㄉ", "ㄊ"),
    ('w', "ㄊ", "ㄉ"),

    ('t', "ㄓ", "ㄔ"),
    ('t', "ㄔ", "ㄓ"),

    ('i', "ㄛ", "ㄞ"),
    ('i', "ㄞ", "ㄛ"),

    ('o', "ㄟ", "ㄢ"),
    ('o', "ㄢ", "ㄟ"),

    ('l', "ㄠ", "ㄤ"),
    ('l', "ㄤ", "ㄠ"),

    ('p', "ㄣ", "ㄦ"),
    ('p', "ㄦ", "ㄣ"),
]

dachen_cp26_switch_special = [
# m_initial != zero || m_middle != zero

    ('b', "ㄖ", "ㄝ"),

    ('n', "ㄙ", "ㄣ"),

# switching between "ㄧ", "ㄚ", and "ㄧㄚ"
# m_middle == 'ㄧ' and m_final != 'ㄚ'
    ('u', "ㄧ", "ㄚ"),
# m_middle != 'ㄧ' and m_final == 'ㄚ'
    ('u', "ㄚ", "ㄧㄚ"),
# m_middle == 'ㄧ' and m_final == "ㄚ"
    ('u', "ㄧㄚ", ""),
# m_middle != zero
    ('u', "*?", "*ㄚ"),

# switching between "ㄩ" and "ㄡ"
# m_final != 'ㄡ'
    ('m', "ㄩ", "ㄡ"),
# m_middle != 'ㄩ'
    ('m', "ㄡ", "ㄩ"),
# m_middle != zero
    ('m', "*?", "*ㄡ"),
]
