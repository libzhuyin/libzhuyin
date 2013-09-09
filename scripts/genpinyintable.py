# -*- coding: utf-8 -*-
# vim:set et sts=4 sw=4:
#
# libzhuyin - Library to deal with zhuyin.
#
# Copyright (C) 2011 Peng Wu <alexepico@gmail.com>
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

import operator
from bopomofo import BOPOMOFO_HANYU_PINYIN_MAP, BOPOMOFO_LUOMA_PINYIN_MAP, BOPOMOFO_SECONDARY_BOPOMOFO_MAP
from pinyintable import *
from correct import *
from chewingkey import gen_table_index


content_table = []
hanyu_pinyin_index = []
luoma_pinyin_index = []
bopomofo_index = []
secondary_bopomofo_index = []


#pinyin table
def filter_pinyin_list():
    for (pinyin, bopomofo, flags, chewing) in gen_pinyin_list():
        (luoma, second) = (None, None)

        if bopomofo in BOPOMOFO_LUOMA_PINYIN_MAP:
            luoma = BOPOMOFO_LUOMA_PINYIN_MAP[bopomofo]

        if bopomofo in BOPOMOFO_SECONDARY_BOPOMOFO_MAP:
            second = BOPOMOFO_SECONDARY_BOPOMOFO_MAP[bopomofo]

        flags = '|'.join(flags)
        chewing = "ChewingKey({0})".format(', '.join(chewing))
        #correct = correct.replace("v", "ü")

        content_table.append((pinyin, bopomofo, luoma, second, chewing))

        if "IS_PINYIN" in flags:
            hanyu_pinyin_index.append((pinyin, flags))
        if luoma:
            luoma_pinyin_index.append((luoma, "IS_PINYIN"))
        if "IS_BOPOMOFO" in flags:
            bopomofo_index.append((bopomofo, flags))
        if second:
            secondary_bopomofo_index.append((second, "IS_PINYIN"))


def sort_all():
    global content_table, hanyu_pinyin_index, luoma_pinyin_index
    global bopomofo_index, secondary_bopomofo_index

    #remove duplicates
    content_table = list(set(content_table))
    hanyu_pinyin_index = list(set(hanyu_pinyin_index))
    luoma_pinyin_index = list(set(luoma_pinyin_index))
    bopomofo_index = list(set(bopomofo_index))
    secondary_bopomofo_index = list(set(secondary_bopomofo_index))

    #define sort function
    sortfunc = operator.itemgetter(0)
    #begin sort
    content_table = sorted(content_table, key=sortfunc)
    #prepend zero item to reserve the invalid item
    content_table.insert(0, ("", "", "", "", "ChewingKey()"))
    #sort index
    hanyu_pinyin_index = sorted(hanyu_pinyin_index, key=sortfunc)
    luoma_pinyin_index = sorted(luoma_pinyin_index, key=sortfunc)
    bopomofo_index = sorted(bopomofo_index, key=sortfunc)
    secondary_bopomofo_index = sorted(secondary_bopomofo_index, key=sortfunc)

'''
def get_sheng_yun(pinyin):
    if pinyin == None:
        return None, None
    if pinyin == "":
        return "", ""
    if pinyin == "ng":
        return "", "ng"
    for i in range(2, 0, -1):
        s = pinyin[:i]
        if s in shengmu_list:
            return s, pinyin[i:]
    return "", pinyin
'''

def gen_content_table():
    entries = []
    for ((pinyin, bopomofo, luoma, second, chewing)) in content_table:
        entry = '{{"{0}", "{1}", "{2}", "{3}" ,{4}}}'.format(pinyin, bopomofo, luoma, second, chewing)
        entries.append(entry)
    return ',\n'.join(entries)


def gen_hanyu_pinyin_index():
    entries = []
    for (pinyin, flags) in hanyu_pinyin_index:
        index = [x[0] for x in content_table].index(pinyin)
        entry = '{{"{0}", {1}, {2}}}'.format(pinyin, flags, index)
        entries.append(entry)
    return ',\n'.join(entries)

def gen_luoma_pinyin_index():
    entries = []
    for (pinyin, flags) in luoma_pinyin_index:
        index = [x[2] for x in content_table].index(pinyin)
        entry = '{{"{0}", {1}, {2}}}'.format(pinyin, flags, index)
        entries.append(entry)
    return ',\n'.join(entries)

def gen_bopomofo_index():
    entries = []
    for (bopomofo, flags) in bopomofo_index:
        pinyin = BOPOMOFO_HANYU_PINYIN_MAP[bopomofo]
        index = [x[0] for x in content_table].index(pinyin)
        entry = '{{"{0}", {1}, {2}}}'.format(bopomofo, flags, index)
        entries.append(entry)
    return ',\n'.join(entries)

def gen_secondary_bopomofo_index():
    entries = []
    for (bopomofo, flags) in secondary_bopomofo_index:
        index = [x[3] for x in content_table].index(bopomofo)
        entry = '{{"{0}", {1}, {2}}}'.format(bopomofo, flags, index)
        entries.append(entry)
    return ',\n'.join(entries)

def check_rule(correct, wrong):
    if '*' not in correct:
        assert '*' not in wrong
    elif correct.endswith('*'):
        assert wrong.endswith('*')
    return True

def gen_chewing_key_table():
    return gen_table_index(content_table)


#init code
filter_pinyin_list()
sort_all()


### main function ###
if __name__ == "__main__":
    #s = gen_content_table() + gen_hanyu_pinyin_index() + gen_bopomofo_index()
    s = gen_content_table() + gen_luoma_pinyin_index() + gen_secondary_bopomofo_index()
    #s = gen_chewing_key_table()
    print(s)
