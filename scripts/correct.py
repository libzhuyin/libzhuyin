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
