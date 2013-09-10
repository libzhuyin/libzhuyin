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


from utils import expand_file
from genpinyintable import gen_content_table, \
    gen_hanyu_pinyin_index, gen_luoma_pinyin_index, \
    gen_bopomofo_index, gen_secondary_bopomofo_index, \
    gen_hsu_bopomofo_index, gen_eten26_bopomofo_index, \
    gen_chewing_key_table

def get_table_content(tablename):
    if tablename == 'CONTENT_TABLE':
        return gen_content_table()
    if tablename == 'HANYU_PINYIN_INDEX':
        return gen_hanyu_pinyin_index()
    if tablename == 'LUOMA_PINYIN_INDEX':
        return gen_luoma_pinyin_index()
    if tablename == 'BOPOMOFO_INDEX':
        return gen_bopomofo_index()
    if tablename == 'SECONDARY_BOPOMOFO_INDEX':
        return gen_secondary_bopomofo_index()
    if tablename == 'HSU_BOPOMOFO_INDEX':
        return gen_hsu_bopomofo_index()
    if tablename == 'ETEN26_BOPOMOFO_INDEX':
        return gen_eten26_bopomofo_index()
    if tablename == 'DIVIDED_TABLE':
        return ''
    if tablename == 'RESPLIT_TABLE':
        return ''
    if tablename == 'TABLE_INDEX':
        return gen_chewing_key_table()


### main function ###
if __name__ == "__main__":
    expand_file("pinyin_parser_table.h.in", get_table_content)
