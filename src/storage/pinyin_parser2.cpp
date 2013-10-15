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


#include "pinyin_parser2.h"
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "stl_lite.h"
#include "pinyin_phrase2.h"
#include "pinyin_custom2.h"
#include "chewing_key.h"
#include "pinyin_parser_table.h"
#include "chewing_table.h"


using namespace pinyin;

static bool check_pinyin_options(pinyin_option_t options, const pinyin_index_item_t * item) {
    guint32 flags = item->m_flags;
    assert (flags & IS_PINYIN);

    /* handle incomplete pinyin. */
    if (flags & PINYIN_INCOMPLETE) {
        if (!(options & PINYIN_INCOMPLETE))
            return false;
    }

#if 0
    /* handle correct pinyin, currently only one flag per item. */
    flags &= PINYIN_CORRECT_ALL;
    options &= PINYIN_CORRECT_ALL;

    if (flags) {
        if ((flags & options) != flags)
            return false;
    }
#endif

    return true;
}

static bool check_chewing_options(pinyin_option_t options, const chewing_index_item_t * item) {
    guint32 flags = item->m_flags;
    assert (flags & IS_BOPOMOFO);

    /* handle incomplete chewing. */
    if (flags & CHEWING_INCOMPLETE) {
        if (!(options & CHEWING_INCOMPLETE))
            return false;
    }

    /* handle correct chewing, currently only one flag per item. */
    flags &= CHEWING_CORRECT_ALL;
    options &= CHEWING_CORRECT_ALL;

    if (flags) {
        if ((flags & options) != flags)
            return false;
    }

    return true;
}


gint _ChewingKey::get_table_index() {
    assert(m_initial <  CHEWING_NUMBER_OF_INITIALS);
    assert(m_middle < CHEWING_NUMBER_OF_MIDDLES);
    assert(m_final < CHEWING_NUMBER_OF_FINALS);

    gint index = chewing_key_table[(m_initial * CHEWING_NUMBER_OF_MIDDLES + m_middle) * CHEWING_NUMBER_OF_FINALS + m_final];
    return index == -1 ? 0 : index;
}

gchar * _ChewingKey::get_pinyin_string(PinyinScheme scheme) {
    assert(m_tone < CHEWING_NUMBER_OF_TONES);
    gint index = get_table_index();
    assert(index < (int) G_N_ELEMENTS(content_table));
    const content_table_item_t & item = content_table[index];

    const char * pinyin_str = NULL;

    switch(scheme) {
    case PINYIN_HANYU:
        pinyin_str = item.m_hanyu_pinyin;
        break;
    case PINYIN_LUOMA:
        pinyin_str = item.m_luoma_pinyin;
        break;
    case PINYIN_SECONDARY_BOPOMOFO:
        pinyin_str = item.m_secondary_bopomofo;
        break;
    default:
        assert(false);
    }

    if (CHEWING_ZERO_TONE == m_tone) {
        return g_strdup(pinyin_str);
    } else {
        return g_strdup_printf("%s%d", pinyin_str, m_tone);
    }
}

gchar * _ChewingKey::get_bopomofo_string() {
    assert(m_tone < CHEWING_NUMBER_OF_TONES);
    gint index = get_table_index();
    assert(index < (int) G_N_ELEMENTS(content_table));
    const content_table_item_t & item = content_table[index];

    if (CHEWING_ZERO_TONE == m_tone) {
        return g_strdup(item.m_bopomofo);
    } else {
        return g_strdup_printf("%s%s", item.m_bopomofo,
                               chewing_tone_table[m_tone]);
    }
}

/* Pinyin Parsers */

/* internal information for pinyin parsers. */
struct parse_value_t{
    ChewingKey m_key;
    ChewingKeyRest m_key_rest;
    gint16 m_num_keys;
    gint16 m_parsed_len;
    gint16 m_last_step;

    /* constructor */
public:
    parse_value_t(){
        m_num_keys = 0;
        m_parsed_len = 0;
        m_last_step = -1;
    }
};

const guint16 max_full_pinyin_length   = 7;  /* include tone. */

const guint16 max_double_pinyin_length = 3;  /* include tone. */

const guint16 max_chewing_length       = 4;  /* include tone. */

const guint16 max_chewing_dachen26_length = 12; /* include tone. */

const guint16 max_utf8_length = 6;

static bool compare_pinyin_less_than(const pinyin_index_item_t & lhs,
                                     const pinyin_index_item_t & rhs){
    return 0 > strcmp(lhs.m_pinyin_input, rhs.m_pinyin_input);
}

static inline bool search_pinyin_index(pinyin_option_t options,
                                       const pinyin_index_item_t * pinyin_index,
                                       size_t len,
                                       const char * pinyin,
                                       ChewingKey & key){
    pinyin_index_item_t item;
    memset(&item, 0, sizeof(item));
    item.m_pinyin_input = pinyin;

    std_lite::pair<const pinyin_index_item_t *,
                   const pinyin_index_item_t *> range;
    range = std_lite::equal_range
        (pinyin_index, pinyin_index + len,
         item, compare_pinyin_less_than);

    guint16 range_len = range.second - range.first;
    assert(range_len <= 1);
    if (range_len == 1) {
        const pinyin_index_item_t * index = range.first;

        if (!check_pinyin_options(options, index))
            return false;

        key = content_table[index->m_table_index].m_chewing_key;
        assert(key.get_table_index() == index->m_table_index);
        return true;
    }

    return false;
}

static bool compare_chewing_less_than(const chewing_index_item_t & lhs,
                                      const chewing_index_item_t & rhs){
    return 0 > strcmp(lhs.m_chewing_input, rhs.m_chewing_input);
}

static inline bool search_chewing_index(pinyin_option_t options,
                                        const chewing_index_item_t * chewing_index,
                                        size_t len,
                                        const char * chewing,
                                        ChewingKey & key){
    chewing_index_item_t item;
    memset(&item, 0, sizeof(item));
    item.m_chewing_input = chewing;

    std_lite::pair<const chewing_index_item_t *,
                   const chewing_index_item_t *> range;
    range = std_lite::equal_range
        (chewing_index, chewing_index + len,
         item, compare_chewing_less_than);

    guint16 range_len = range.second - range.first;
    assert (range_len <= 1);

    if (range_len == 1) {
        const chewing_index_item_t * index = range.first;

        if (!check_chewing_options(options, index))
            return false;

        key = content_table[index->m_table_index].m_chewing_key;
        assert(key.get_table_index() == index->m_table_index);
        return true;
    }

    return false;
}

/* Full Pinyin Parser */
FullPinyinParser2::FullPinyinParser2 (){
    m_pinyin_index = NULL; m_pinyin_index_len = 0;
    m_parse_steps = g_array_new(TRUE, FALSE, sizeof(parse_value_t));

    set_scheme(PINYIN_DEFAULT);
}

bool FullPinyinParser2::parse_one_key (pinyin_option_t options,
                                       ChewingKey & key,
                                       const char * pinyin, int len) const {
    /* "'" are not accepted in parse_one_key. */
    gchar * input = g_strndup(pinyin, len);
    assert(NULL == strchr(input, '\''));

    guint16 tone = CHEWING_ZERO_TONE; guint16 tone_pos = 0;
    guint16 parsed_len = len;
    key = ChewingKey();

    if (options & USE_TONE) {
        /* find the tone in the last character. */
        char chr = input[parsed_len - 1];
        if ( '0' < chr && chr <= '5' ) {
            tone = chr - '0';
            parsed_len --;
            tone_pos = parsed_len;
        }
    }

    /* parse pinyin core staff here. */

    /* Note: optimize here? */
    input[parsed_len] = '\0';
    if (!search_pinyin_index(options, m_pinyin_index, m_pinyin_index_len,
                             input, key)) {
        g_free(input);
        return false;
    }

    if (options & USE_TONE) {
        /* post processing tone. */
        if ( parsed_len == tone_pos ) {
            if (tone != CHEWING_ZERO_TONE) {
                key.m_tone = tone;
                parsed_len ++;
            }
        }
    }

    g_free(input);
    return parsed_len == len;
}


int FullPinyinParser2::parse (pinyin_option_t options, ChewingKeyVector & keys,
                              ChewingKeyRestVector & key_rests,
                              const char *str, int len) const {
    int i;
    /* clear arrays. */
    g_array_set_size(keys, 0);
    g_array_set_size(key_rests, 0);

    /* init m_parse_steps, and prepare dynamic programming. */
    int step_len = len + 1;
    g_array_set_size(m_parse_steps, 0);
    parse_value_t value;
    for (i = 0; i < step_len; ++i) {
        g_array_append_val(m_parse_steps, value);
    }

    size_t next_sep = 0;
    gchar * input = g_strndup(str, len);
    parse_value_t * curstep = NULL, * nextstep = NULL;

    for (i = 0; i < len; ++i) {
        if (input[i] == '\'') {
            curstep = &g_array_index(m_parse_steps, parse_value_t, i);
            nextstep = &g_array_index(m_parse_steps, parse_value_t, i + 1);

            /* propagate current step into next step. */
            nextstep->m_key = ChewingKey();
            nextstep->m_key_rest = ChewingKeyRest();
            nextstep->m_num_keys = curstep->m_num_keys;
            nextstep->m_parsed_len = curstep->m_parsed_len + 1;
            nextstep->m_last_step = i;
            next_sep = 0;
            continue;
        }

        /* forward to next "'" */
        if ( 0 == next_sep ) {
            int k;
            for (k = i;  k < len; ++k) {
                if (input[k] == '\'')
                    break;
            }
            next_sep = k;
        }

        /* dynamic programming here. */
        /* for (size_t m = i; m < next_sep; ++m) */
        {
            size_t m = i;
            curstep = &g_array_index(m_parse_steps, parse_value_t, m);
            size_t try_len = std_lite::min
                (m + max_full_pinyin_length, next_sep);
            for (size_t n = m + 1; n < try_len + 1; ++n) {
                nextstep = &g_array_index(m_parse_steps, parse_value_t, n);

                /* gen next step */
                const char * onepinyin = input + m;
                gint16 onepinyinlen = n - m;
                value = parse_value_t();

                ChewingKey key; ChewingKeyRest rest;
                bool parsed = parse_one_key
                    (options, key, onepinyin, onepinyinlen);
                rest.m_raw_begin = m; rest.m_raw_end = n;
                if (!parsed)
                    continue;

                //printf("onepinyin:%s len:%d\n", onepinyin, onepinyinlen);

                value.m_key = key; value.m_key_rest = rest;
                value.m_num_keys = curstep->m_num_keys + 1;
                value.m_parsed_len = curstep->m_parsed_len + onepinyinlen;
                value.m_last_step = m;

                /* save next step */
                /* no previous result */
                if (-1 == nextstep->m_last_step)
                    *nextstep = value;
                /* prefer the longest pinyin */
                if (value.m_parsed_len > nextstep->m_parsed_len)
                    *nextstep = value;
                /* prefer the shortest keys with the same pinyin length */
                if (value.m_parsed_len == nextstep->m_parsed_len &&
                    value.m_num_keys < nextstep->m_num_keys)
                    *nextstep = value;

            }
        }
    }

    /* final step for back tracing. */
    gint16 parsed_len = final_step(step_len, keys, key_rests);

    g_free(input);
    return parsed_len;
}

int FullPinyinParser2::final_step(size_t step_len, ChewingKeyVector & keys,
                                  ChewingKeyRestVector & key_rests) const{
    int i;
    gint16 parsed_len = 0;
    parse_value_t * curstep = NULL;

    /* find longest match, which starts from the beginning of input. */
    for (i = step_len - 1; i >= 0; --i) {
        curstep = &g_array_index(m_parse_steps, parse_value_t, i);
        if (i == curstep->m_parsed_len)
            break;
    }
    /* prepare saving. */
    parsed_len = curstep->m_parsed_len;
    gint16 num_keys = curstep->m_num_keys;
    g_array_set_size(keys, num_keys);
    g_array_set_size(key_rests, num_keys);

    /* save the match. */
    while (curstep->m_last_step != -1) {
        gint16 pos = curstep->m_num_keys - 1;

        /* skip "'" */
        if (0 != curstep->m_key.get_table_index()) {
            ChewingKey * key = &g_array_index(keys, ChewingKey, pos);
            ChewingKeyRest * rest = &g_array_index
                (key_rests, ChewingKeyRest, pos);
            *key = curstep->m_key; *rest = curstep->m_key_rest;
        }

        /* back ward */
        curstep = &g_array_index(m_parse_steps, parse_value_t,
                                 curstep->m_last_step);
    }
    return parsed_len;
}

bool FullPinyinParser2::set_scheme(PinyinScheme scheme){
    switch(scheme){
    case PINYIN_HANYU:
        m_pinyin_index = hanyu_pinyin_index;
        m_pinyin_index_len = G_N_ELEMENTS(hanyu_pinyin_index);
        break;
    case PINYIN_LUOMA:
        m_pinyin_index = luoma_pinyin_index;
        m_pinyin_index_len = G_N_ELEMENTS(luoma_pinyin_index);
        break;
    case PINYIN_SECONDARY_BOPOMOFO:
        m_pinyin_index = secondary_bopomofo_index;
        m_pinyin_index_len = G_N_ELEMENTS(secondary_bopomofo_index);
        break;
    default:
        assert(false);
    }
    return true;
}

#if 0

static const char * pinyin_symbols[27] = {
    "a", "b", "c", "d", "e", "f", "g",
    "h", "i", "j", "k", "l", "m", "n",
    "o", "p", "q", "r", "s", "t",
    "u", "v", "w", "x", "y", "z",
    "'"
};

bool FullPinyinParser2::in_chewing_scheme(pinyin_option_t options,
                                          const char key,
                                          const char ** symbol) const {
    int id;
    if ('a' <= key && key <= 'z') {
        id = key - 'a';
        *symbol = pinyin_symbols[id];
        return true;
    }

    if ('\'' == key) {
        id = 26;
        *symbol = pinyin_symbols[id];
        return true;
    }

    return false;
}

#endif

/* the chewing string must be freed with g_free. */
static bool search_chewing_symbols(const chewing_symbol_item_t * symbol_table,
                                   const char key, const char ** chewing) {
    *chewing = "";
    /* just iterate the table, as we only have < 50 items. */
    while (symbol_table->m_input != '\0') {
        if (symbol_table->m_input == key) {
            *chewing = symbol_table->m_chewing;
            return true;
        }
        symbol_table ++;
    }
    return false;
}

static bool search_chewing_tones(const chewing_tone_item_t * tone_table,
                                 const char key, unsigned char * tone) {
    *tone = CHEWING_ZERO_TONE;
    /* just iterate the table, as we only have < 10 items. */
    while (tone_table->m_input != '\0') {
        if (tone_table->m_input == key) {
            *tone = tone_table->m_tone;
            return true;
        }
        tone_table ++;
    }
    return false;
}

#if 0
bool ChewingSimpleParser2::parse_one_key(pinyin_option_t options,
                                         ChewingKey & key,
                                         const char * str, int len) const {
    options &= ~PINYIN_AMB_ALL;
    unsigned char tone = CHEWING_ZERO_TONE;

    int symbols_len = len;
    /* probe whether the last key is tone key in str. */
    if (options & USE_TONE) {
        char ch = str[len - 1];
        /* remove tone from input */
        if (search_chewing_tones(m_tone_table, ch, &tone))
            symbols_len --;
    }

    int i;
    gchar * chewing = NULL; const char * onechar = NULL;

    /* probe the possible chewing map in the rest of str. */
    for (i = 0; i < symbols_len; ++i) {
        if (!search_chewing_symbols(m_symbol_table, str[i], &onechar)) {
            g_free(chewing);
            return false;
        }

        if (!chewing) {
            chewing = g_strdup(onechar);
        } else {
            gchar * tmp = chewing;
            chewing = g_strconcat(chewing, onechar, NULL);
            g_free(tmp);
        }
    }

    /* search the chewing in the chewing index table. */
    if (chewing && search_chewing_index(options, bopomofo_index,
                                        G_N_ELEMENTS(bopomofo_index),
                                        chewing, key)) {
        /* save back tone if available. */
        key.m_tone = tone;
        g_free(chewing);
        return true;
    }

    g_free(chewing);
    return false;
}

#endif

bool ChewingDiscreteParser2::parse_one_key(pinyin_option_t options,
                                           ChewingKey & key,
                                           const char * str, int len) const {
    if (0 == len)
        return false;

    options &= ~PINYIN_AMB_ALL;

    int index = 0;
    const char * initial = "";
    const char * middle = "";
    const char * final = "";
    unsigned char tone = CHEWING_ZERO_TONE;

    /* probe initial */
    if (search_chewing_symbols(m_initial_table, str[index], &initial)) {
        index++;
    }

    if (index == len)
        goto probe;

    /* probe middle */
    if (search_chewing_symbols(m_middle_table, str[index], &middle)) {
        index++;
    }

    if (index == len)
        goto probe;

    /* probe final */
    if (search_chewing_symbols(m_final_table, str[index], &final)) {
        index++;
    }

    if (index == len)
        goto probe;

    /* probe tone */
    if (options & USE_TONE) {
        if (search_chewing_tones(m_tone_table, str[index], &tone)) {
            index ++;
        }
    }

probe:
    gchar * chewing = g_strconcat(initial, middle, final, NULL);

    /* search the chewing in the chewing index table. */
    if (index == len && search_chewing_index(options, m_chewing_index,
                                             m_chewing_index_len,
                                             chewing, key)) {
        /* save back tone if available. */
        key.m_tone = tone;
        g_free(chewing);
        return true;
    }

    g_free(chewing);
    return false;
}

/* only characters in chewing keyboard scheme are accepted here. */
int ChewingDiscreteParser2::parse(pinyin_option_t options,
                                  ChewingKeyVector & keys,
                                  ChewingKeyRestVector & key_rests,
                                  const char *str, int len) const {
    /* add keyboard mapping specific options. */
    options |= m_options;

    g_array_set_size(keys, 0);
    g_array_set_size(key_rests, 0);

    int maximum_len = 0; int i;
    /* probe the longest possible chewing string. */
    for (i = 0; i < len; ++i) {
        if (!in_chewing_scheme(options, str[i], NULL))
            break;
    }
    maximum_len = i;

    /* maximum forward match for chewing. */
    int parsed_len = 0;
    while (parsed_len < maximum_len) {
        const char * cur_str = str + parsed_len;
        i = std_lite::min(maximum_len - parsed_len,
                          (int)max_chewing_length);

        ChewingKey key; ChewingKeyRest key_rest;
        for (; i > 0; --i) {
            bool success = parse_one_key(options, key, cur_str, i);
            if (success)
                break;
        }

        if (0 == i)        /* no more possible chewings. */
            break;

        key_rest.m_raw_begin = parsed_len; key_rest.m_raw_end = parsed_len + i;
        parsed_len += i;

        /* save the pinyin. */
        g_array_append_val(keys, key);
        g_array_append_val(key_rests, key_rest);
    }

    return parsed_len;
}

bool ChewingDiscreteParser2::set_scheme(ChewingScheme scheme) {
    m_options = 0;

#define INIT_PARSER(index, table) {                     \
        m_chewing_index = index;                        \
        m_chewing_index_len = G_N_ELEMENTS(index);      \
        m_initial_table = chewing_##table##_initials;   \
        m_middle_table  = chewing_##table##_middles;    \
        m_final_table   = chewing_##table##_finals;     \
        m_tone_table    = chewing_##table##_tones;      \
    }

    switch(scheme) {
    case CHEWING_STANDARD:
        INIT_PARSER(bopomofo_index, standard);
        break;
    case CHEWING_HSU:
        m_options = HSU_CORRECT;
        INIT_PARSER(hsu_bopomofo_index, hsu);
        break;
    case CHEWING_IBM:
        INIT_PARSER(bopomofo_index, ibm);
        break;
    case CHEWING_GINYIEH:
        INIT_PARSER(bopomofo_index, ginyieh);
        break;
    case CHEWING_ETEN:
        INIT_PARSER(bopomofo_index, eten);
        break;
    case CHEWING_ETEN26:
        m_options = ETEN26_CORRECT;
        INIT_PARSER(eten26_bopomofo_index, eten26);
        break;
    case CHEWING_STANDARD_DVORAK:
        INIT_PARSER(bopomofo_index, standard_dvorak);
        break;
    case CHEWING_HSU_DVORAK:
        m_options = HSU_CORRECT;
        INIT_PARSER(hsu_bopomofo_index, hsu_dvorak);
        break;
    default:
        assert(FALSE);
    }

#undef INIT_PARSER

    return true;
}

bool ChewingDiscreteParser2::in_chewing_scheme(pinyin_option_t options,
                                               const char key,
                                               const char ** symbol) const {
    const gchar * chewing = NULL;
    unsigned char tone = CHEWING_ZERO_TONE;

    if (search_chewing_symbols(m_initial_table, key, &chewing)) {
        if (symbol)
            *symbol = chewing;
        return true;
    }

    if (search_chewing_symbols(m_middle_table, key, &chewing)) {
        if (symbol)
            *symbol = chewing;
        return true;
    }

    if (search_chewing_symbols(m_final_table, key, &chewing)) {
        if (symbol)
            *symbol = chewing;
        return true;
    }

    if (!(options & USE_TONE))
        return false;

    if (search_chewing_tones(m_tone_table, key, &tone)) {
        if (symbol)
            *symbol = chewing_tone_table[tone];
        return true;
    }

    return false;
}

static int search_chewing_symbols2(const chewing_symbol_item_t * symbol_table,
                                   const char key,
                                   const char ** first,
                                   const char ** second) {
    int num = 0;
    *first = NULL; *second = NULL;

    /* just iterate the table, as we only have < 50 items. */
    while (symbol_table->m_input != '\0') {
        if (symbol_table->m_input == key) {
            ++num;
            if (NULL == *first) {
                *first = symbol_table->m_chewing;
            } else {
                *second = symbol_table->m_chewing;
            }
        }

        /* search done */
        if (symbol_table->m_input > key)
            break;

        symbol_table++;
    }

    assert(0 <= num && num <= 2);
    return num;
}

ChewingDaChenCP26Parser2::ChewingDaChenCP26Parser2() {
    m_chewing_index = bopomofo_index;
    m_chewing_index_len = G_N_ELEMENTS(bopomofo_index);

    m_initial_table = chewing_dachen_cp26_initials;
    m_middle_table  = chewing_dachen_cp26_middles;
    m_final_table   = chewing_dachen_cp26_finals;
    m_tone_table    = chewing_dachen_cp26_tones;
}

bool ChewingDaChenCP26Parser2::parse_one_key(pinyin_option_t options,
                                             ChewingKey & key,
                                             const char *str, int len) const {
    if (0 == len)
        return false;

    options &= ~PINYIN_AMB_ALL;

    const char * initial = "";
    const char * middle  = "";
    const char * final   = "";
    unsigned char tone   = CHEWING_ZERO_TONE;

    gchar * input = g_strndup(str, len);
    int index = 0;

    char ch;
    const char * first = NULL;
    const char * second = NULL;

    /* probe whether the last key is tone key in input. */
    if (options & USE_TONE) {
        ch = input[len - 1];
        /* remove tone from input */
        if (search_chewing_tones(m_tone_table, ch, &tone))
            len --;
    }

    if (0 == len)
        return false;

    int i; int choice;

    /* probe initial */
    do {
        ch = input[index];
        if (search_chewing_symbols2(m_initial_table, ch, &first, &second)) {
            index ++;
            if (NULL == second) {
                initial = first;
                break;
            } else {
                choice = 0;
                /* zero out the same char */
                for (i = index; i < len; ++i) {
                    if (input[i] == ch) {
                        input[i] = '\0';
                        choice ++;
                    }
                }
                choice = choice % 2;
                if (0 == choice)
                    initial = first;
                if (1 == choice)
                    initial = second;
            }
        }
    } while (0);

    /* skip zeros */
    for (; index < len; index ++) {
        if ('\0' != input[index])
            break;
    }

    if (index == len)
        goto probe;

    first = NULL; second = NULL;
    /* probe middle */
    do {
        ch = input[index];
        /* handle 'u' */
        if ('u' == ch) {
            index ++;
            choice = 0;
            /* zero out the same char */
            for (i = index; i < len; ++i) {
                if ('u' == input[i]) {
                    input[i] = '\0';
                    choice ++;
                }
                choice = choice % 3;
                if (0 == choice)
                    middle = "ㄧ";
                if (1 == choice)
                    final = "ㄚ";
                if (2 == choice) {
                    middle = "ㄧ";
                    final = "ㄚ";
                }
            }
        }
        /* handle 'm' */
        if ('m' == ch) {
            index ++;
            choice = 0;
            /* zero out the same char */
            for (i = index; i < len; ++i) {
                if ('m' == input[i]) {
                    input[i] = '\0';
                    choice ++;
                }
                choice = choice % 2;
                if (0 == choice)
                    middle = "ㄩ";
                if (1 == choice)
                    final = "ㄡ";
            }
        }
        if (search_chewing_symbols2(m_middle_table, ch, &first, &second)) {
            assert(NULL == second);
            index ++;
            middle = first;
        }
    } while(0);

    /* skip zeros */
    for (; index < len; index ++) {
        if ('\0' != input[index])
            break;
    }

    if (index == len)
        goto probe;

    /* probe final */
    do {
        /* for 'u' and 'm' */
        if (0 != strlen(final))
            break;

        ch = input[index];
        if (search_chewing_symbols2(m_final_table, ch, &first, &second)) {
            index ++;
            if (NULL == second) {
                final = first;
                break;
            } else {
                choice = 0;
                /* zero out the same char */
                for (i = index; i < len; ++i) {
                    if (input[i] == ch) {
                        input[i] = '\0';
                        choice ++;
                    }
                }
                choice = choice % 2;
                if (0 == choice)
                    final = first;
                if (1 == choice)
                    final = second;
            }
        }
    } while(0);

    /* skip zeros */
    for (; index < len; index ++) {
        if ('\0' != input[index])
            break;
    }

    if (index == len)
        goto probe;

probe:
    gchar * chewing = g_strconcat(initial, middle, final, NULL);

    /* search the chewing in the chewing index table. */
    if (index == len && search_chewing_index(options, m_chewing_index,
                                             m_chewing_index_len,
                                             chewing, key)) {
        /* save back tone if available. */
        key.m_tone = tone;
        g_free(chewing);
        g_free(input);
        return true;
    }

    g_free(chewing);
    g_free(input);
    return false;
}

int ChewingDaChenCP26Parser2::parse(pinyin_option_t options,
                                    ChewingKeyVector & keys,
                                    ChewingKeyRestVector & key_rests,
                                    const char *str, int len) const {
    g_array_set_size(keys, 0);
    g_array_set_size(key_rests, 0);

    int maximum_len = 0; int i;
    /* probe the longest possible chewing string. */
    for (i = 0; i < len; ++i) {
        if (!in_chewing_scheme(options, str[i], NULL))
            break;
    }
    maximum_len = i;

    /* maximum forward match for chewing. */
    int parsed_len = 0;
    const char * cur_str = NULL;
    ChewingKey key; ChewingKeyRest key_rest;

    while (parsed_len < maximum_len) {
        cur_str = str + parsed_len;
        i = std_lite::min(maximum_len - parsed_len,
                          (int)max_chewing_dachen26_length);

        for (; i > 0; --i) {
            bool success = parse_one_key(options, key, cur_str, i);
            if (success)
                break;
        }

        if (0 == i)        /* no more possible chewings. */
            break;

        key_rest.m_raw_begin = parsed_len; key_rest.m_raw_end = parsed_len + i;
        parsed_len += i;

        /* save the pinyin. */
        g_array_append_val(keys, key);
        g_array_append_val(key_rests, key_rest);
    }

    /* for the last partial input */
    options |= CHEWING_INCOMPLETE;

    cur_str = str + parsed_len;
    i = std_lite::min(maximum_len - parsed_len,
                      (int) max_chewing_dachen26_length);
    for (; i > 0; --i) {
        bool success = parse_one_key(options, key, cur_str, i);
        if (success)
            break;
    }

    if (i > 0) { /* found one */
        key_rest.m_raw_begin = parsed_len; key_rest.m_raw_end = parsed_len + i;
        parsed_len += i;

        /* save the pinyin. */
        g_array_append_val(keys, key);
        g_array_append_val(key_rests, key_rest);
    }

    return parsed_len;
}


bool ChewingDaChenCP26Parser2::in_chewing_scheme(pinyin_option_t options,
                                                 const char key,
                                                 const char ** symbol) const {
    const gchar * chewing = NULL;
    unsigned char tone = CHEWING_ZERO_TONE;

    if (search_chewing_symbols(m_initial_table, key, &chewing)) {
        if (symbol)
            *symbol = chewing;
        return true;
    }

    if (search_chewing_symbols(m_middle_table, key, &chewing)) {
        if (symbol)
            *symbol = chewing;
        return true;
    }

    if (search_chewing_symbols(m_final_table, key, &chewing)) {
        if (symbol)
            *symbol = chewing;
        return true;
    }

    if (!(options & USE_TONE))
        return false;

    if (search_chewing_tones(m_tone_table, key, &tone)) {
        if (symbol)
            *symbol = chewing_tone_table[tone];
        return true;
    }

    return false;
}

ChewingDirectParser2::ChewingDirectParser2 (){
    m_chewing_index = bopomofo_index;
    m_chewing_index_len = G_N_ELEMENTS(bopomofo_index);
}

bool ChewingDirectParser2::parse_one_key(pinyin_option_t options,
                                         ChewingKey & key,
                                         const char *str, int len) const {
    options &= ~PINYIN_AMB_ALL;
    unsigned char tone = CHEWING_ZERO_TONE;

    if (0 == len)
        return false;

    const gchar * last_char = NULL;
    for (const char * p = str; p < str + len; p = g_utf8_next_char(p)) {
        last_char = p;
    }

    /* probe tone first. */
    if (options & USE_TONE) {
        gchar buffer[max_utf8_length + 1];
        memset(buffer, 0, sizeof(buffer));
        g_utf8_strncpy(buffer, last_char, 1);

        /* for loop chewing_tone_table. */
        int i = 1;
        for (; i < (int) G_N_ELEMENTS(chewing_tone_table); ++i) {
            const char * symbol = chewing_tone_table[i];
            if (0 == strcmp(symbol, buffer)) {
                tone = i;
                len -= strlen(buffer);
                break;
            }
        }
    }

    gchar * chewing = g_strndup(str, len);
    /* search the chewing in the chewing index table. */
    if (len && search_chewing_index(options, m_chewing_index,
                                    m_chewing_index_len, chewing, key)) {
        /* save back tone if available. */
        key.m_tone = tone;
        g_free(chewing);
        return true;
    }

    g_free(chewing);
    return false;
}

int ChewingDirectParser2::parse(pinyin_option_t options,
                                ChewingKeyVector & keys,
                                ChewingKeyRestVector & key_rests,
                                const char *str, int len) const {
    g_array_set_size(keys, 0);
    g_array_set_size(key_rests, 0);

    ChewingKey key; ChewingKeyRest key_rest;

    int parsed_len = 0;
    int i = 0, cur = 0, next = 0;
    while (cur < len) {
        /* probe next position */
        for (i = cur; i < len; ++i) {
            if (' ' == str[i] || '\'' == str[i])
                break;
        }
        next = i;

        if (parse_one_key(options, key, str + cur, next - cur)) {
            key_rest.m_raw_begin = cur; key_rest.m_raw_end = next;

            /* save the pinyin. */
            g_array_append_val(keys, key);
            g_array_append_val(key_rests, key_rest);
        } else {
            return parsed_len;
        }

        /* skip consecutive spaces */
        for (i = next; i < len; ++i) {
            if (' ' != str[i] && '\'' != str[i])
                break;
        }

        cur = i;
        parsed_len = i;
    }

    return parsed_len;
}
