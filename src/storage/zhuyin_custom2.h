/* 
 *  libzhuyin
 *  Library to deal with zhuyin.
 *  
 *  Copyright (C) 2011 Peng Wu <alexepico@gmail.com>
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ZHUYIN_CUSTOM2_H
#define ZHUYIN_CUSTOM2_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * ZhuyinTableFlag:
 */
enum ZhuyinTableFlag{
    IS_BOPOMOFO = 1U << 1,
    IS_PINYIN = 1U << 2,
    PINYIN_INCOMPLETE = 1U << 3,
    CHEWING_INCOMPLETE = 1U << 4,
    USE_TONE = 1U << 5,
    FORCE_TONE = 1U << 6,
    HSU_CORRECT = 1U << 7,
    ETEN26_CORRECT = 1U << 8,
    DYNAMIC_ADJUST = 1U << 9,
    ZHUYIN_CORRECT_ALL = HSU_CORRECT|ETEN26_CORRECT
};

/**
 * ZhuyinAmbiguity2:
 *
 * The enums of zhuyin ambiguities.
 *
 */
enum ZhuyinAmbiguity2{
    ZHUYIN_AMB_C_CH = 1U << 12,
    ZHUYIN_AMB_S_SH = 1U << 13,
    ZHUYIN_AMB_Z_ZH = 1U << 14,
    ZHUYIN_AMB_F_H = 1U << 15,
    ZHUYIN_AMB_G_K = 1U << 16,
    ZHUYIN_AMB_L_N = 1U << 17,
    ZHUYIN_AMB_L_R = 1U << 18,
    ZHUYIN_AMB_AN_ANG = 1U << 19,
    ZHUYIN_AMB_EN_ENG = 1U << 20,
    ZHUYIN_AMB_IN_ING = 1U << 21,
    ZHUYIN_AMB_ALL = 0x3FFU << 12
};

/**
 * @brief enums of Pinyin Schemes.
 */

enum FullPinyinScheme
{
    FULL_PINYIN_HANYU = 1,
    FULL_PINYIN_LUOMA = 2,
    FULL_PINYIN_SECONDARY_BOPOMOFO = 3,
    FULL_PINYIN_DEFAULT = FULL_PINYIN_HANYU
};

/**
 * @brief enums of Chewing Schemes.
 */
enum ChewingScheme
{
    CHEWING_STANDARD = 1,
    CHEWING_HSU      = 2,
    CHEWING_IBM      = 3,
    CHEWING_GINYIEH  = 4,
    CHEWING_ETEN     = 5,
    CHEWING_ETEN26   = 6,
    CHEWING_STANDARD_DVORAK = 7,
    CHEWING_HSU_DVORAK = 8,
    CHEWING_DACHEN_CP26 = 9,
    CHEWING_DEFAULT  = CHEWING_STANDARD
};

G_END_DECLS

#endif
