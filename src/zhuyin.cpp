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


#include "zhuyin.h"
#include <stdio.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include "zhuyin_internal.h"


using namespace zhuyin;

/* a glue layer for input method integration. */

typedef GArray * CandidateVector; /* GArray of lookup_candidate_t */

struct _zhuyin_context_t{
    pinyin_option_t m_options;

    ZhuyinScheme m_full_pinyin_scheme;
    FullPinyinParser2 * m_full_pinyin_parser;
    ChewingParser2 * m_chewing_parser;

    FacadeChewingTable * m_pinyin_table;
    FacadePhraseTable2 * m_phrase_table;
    FacadePhraseIndex * m_phrase_index;
    Bigram * m_system_bigram;
    Bigram * m_user_bigram;

    PinyinLookup2 * m_pinyin_lookup;
    PhraseLookup * m_phrase_lookup;

    char * m_system_dir;
    char * m_user_dir;
    bool m_modified;

    SystemTableInfo m_system_table_info;
};

struct _zhuyin_instance_t{
    zhuyin_context_t * m_context;
    gchar * m_raw_user_input;
    TokenVector m_prefixes;
    ChewingKeyVector m_pinyin_keys;
    ChewingKeyRestVector m_pinyin_key_rests;
    size_t m_parsed_len;
    CandidateConstraints m_constraints;
    MatchResults m_match_results;
    CandidateVector m_candidates;
};

struct _lookup_candidate_t{
    lookup_candidate_type_t m_candidate_type;
    gchar * m_phrase_string;
    phrase_token_t m_token;
    ChewingKeyRest m_orig_rest;
    gchar * m_new_pinyins;
    guint32 m_freq; /* the amplifed gfloat numerical value. */
public:
    _lookup_candidate_t() {
        m_candidate_type = NORMAL_CANDIDATE_AFTER_CURSOR;
        m_phrase_string = NULL;
        m_token = null_token;
        m_new_pinyins = NULL;
        m_freq = 0;
    }
};

struct _import_iterator_t{
    zhuyin_context_t * m_context;
    guint8 m_phrase_index;
};


static bool check_format(zhuyin_context_t * context){
    const char * userdir = context->m_user_dir;

    UserTableInfo user_table_info;
    gchar * filename = g_build_filename
        (userdir, USER_TABLE_INFO, NULL);
    user_table_info.load(filename);
    g_free(filename);

    bool exists = user_table_info.is_conform
        (&context->m_system_table_info);

    if (exists)
        return exists;

    const pinyin_table_info_t * phrase_files =
        context->m_system_table_info.get_table_info();

    /* clean up files, if version mis-matches. */
    for (size_t i = 1; i < PHRASE_INDEX_LIBRARY_COUNT; ++i) {
        const pinyin_table_info_t * table_info = phrase_files + i;

        if (NOT_USED == table_info->m_file_type)
            continue;

        if (NULL == table_info->m_user_filename)
            continue;

        const char * userfilename = table_info->m_user_filename;

        /* remove dbin file. */
        filename = g_build_filename(userdir, userfilename, NULL);
        unlink(filename);
        g_free(filename);
    }

    filename = g_build_filename
        (userdir, USER_PINYIN_INDEX, NULL);
    unlink(filename);
    g_free(filename);

    filename = g_build_filename
        (userdir, USER_PHRASE_INDEX, NULL);
    unlink(filename);
    g_free(filename);

    filename = g_build_filename
        (userdir, USER_BIGRAM, NULL);
    unlink(filename);
    g_free(filename);

    return exists;
}

static bool mark_version(zhuyin_context_t * context){
    const char * userdir = context->m_user_dir;

    UserTableInfo user_table_info;
    user_table_info.make_conform(&context->m_system_table_info);

    gchar * filename = g_build_filename
        (userdir, USER_TABLE_INFO, NULL);
    bool retval = user_table_info.save(filename);
    g_free(filename);

    return retval;
}

zhuyin_context_t * zhuyin_init(const char * systemdir, const char * userdir){
    zhuyin_context_t * context = new zhuyin_context_t;

    context->m_options = USE_TONE;

    context->m_system_dir = g_strdup(systemdir);
    context->m_user_dir = g_strdup(userdir);
    context->m_modified = false;

    gchar * filename = g_build_filename
        (context->m_system_dir, SYSTEM_TABLE_INFO, NULL);
    if (!context->m_system_table_info.load(filename)) {
        fprintf(stderr, "load %s failed!\n", filename);
        return NULL;
    }
    g_free(filename);


    check_format(context);

    context->m_full_pinyin_scheme = FULL_PINYIN_DEFAULT;
    context->m_full_pinyin_parser = new FullPinyinParser2;
    context->m_chewing_parser = new ChewingSimpleParser2;

    /* load chewing table. */
    context->m_pinyin_table = new FacadeChewingTable;

    /* load system chewing table. */
    MemoryChunk * chunk = new MemoryChunk;
    filename = g_build_filename
        (context->m_system_dir, SYSTEM_PINYIN_INDEX, NULL);
    if (!chunk->load(filename)) {
        fprintf(stderr, "open %s failed!\n", filename);
        return NULL;
    }
    g_free(filename);

    /* load user chewing table */
    MemoryChunk * userchunk = new MemoryChunk;
    filename = g_build_filename
        (context->m_user_dir, USER_PINYIN_INDEX, NULL);
    if (!userchunk->load(filename)) {
        /* hack here: use local Chewing Table to create empty memory chunk. */
        ChewingLargeTable table(context->m_options);
        table.store(userchunk);
    }
    g_free(filename);

    context->m_pinyin_table->load(context->m_options, chunk, userchunk);

    /* load phrase table */
    context->m_phrase_table = new FacadePhraseTable2;

    /* load system phrase table */
    chunk = new MemoryChunk;
    filename = g_build_filename
        (context->m_system_dir, SYSTEM_PHRASE_INDEX, NULL);
    if (!chunk->load(filename)) {
        fprintf(stderr, "open %s failed!\n", filename);
        return NULL;
    }
    g_free(filename);

    /* load user phrase table */
    userchunk = new MemoryChunk;
    filename = g_build_filename
        (context->m_user_dir, USER_PHRASE_INDEX, NULL);
    if (!userchunk->load(filename)) {
        /* hack here: use local Phrase Table to create empty memory chunk. */
        PhraseLargeTable2 table;
        table.store(userchunk);
    }
    g_free(filename);

    context->m_phrase_table->load(chunk, userchunk);

    context->m_phrase_index = new FacadePhraseIndex;

    /* hack here: directly call load phrase library. */
    zhuyin_load_phrase_library(context, TSI_DICTIONARY);

    context->m_system_bigram = new Bigram;
    filename = g_build_filename(context->m_system_dir, SYSTEM_BIGRAM, NULL);
    context->m_system_bigram->attach(filename, ATTACH_READONLY);
    g_free(filename);

    context->m_user_bigram = new Bigram;
    filename = g_build_filename(context->m_user_dir, USER_BIGRAM, NULL);
    context->m_user_bigram->load_db(filename);
    g_free(filename);

    gfloat lambda = context->m_system_table_info.get_lambda();

    context->m_pinyin_lookup = new PinyinLookup2
        ( lambda, context->m_options,
          context->m_pinyin_table, context->m_phrase_index,
          context->m_system_bigram, context->m_user_bigram);

    context->m_phrase_lookup = new PhraseLookup
        (lambda,
         context->m_phrase_table, context->m_phrase_index,
         context->m_system_bigram, context->m_user_bigram);

    return context;
}

bool zhuyin_load_phrase_library(zhuyin_context_t * context,
                                guint8 index){
    if (!(index < PHRASE_INDEX_LIBRARY_COUNT))
        return false;

    /* check whether the sub phrase index is already loaded. */
    PhraseIndexRange range;
    int retval = context->m_phrase_index->get_range(index, range);
    if (ERROR_OK == retval)
        return false;

    const pinyin_table_info_t * phrase_files =
        context->m_system_table_info.get_table_info();

    const pinyin_table_info_t * table_info = phrase_files + index;

    if (SYSTEM_FILE == table_info->m_file_type ||
        DICTIONARY == table_info->m_file_type) {
        /* system phrase library */
        MemoryChunk * chunk = new MemoryChunk;

        const char * systemfilename = table_info->m_system_filename;
        /* check bin file in system dir. */
        gchar * chunkfilename = g_build_filename(context->m_system_dir,
                                                 systemfilename, NULL);
        chunk->load(chunkfilename);
        g_free(chunkfilename);

        context->m_phrase_index->load(index, chunk);

        const char * userfilename = table_info->m_user_filename;

        chunkfilename = g_build_filename(context->m_user_dir,
                                         userfilename, NULL);

        MemoryChunk * log = new MemoryChunk;
        log->load(chunkfilename);
        g_free(chunkfilename);

        /* merge the chunk log. */
        context->m_phrase_index->merge(index, log);
        return true;
    }

    if (USER_FILE == table_info->m_file_type) {
        /* user phrase library */
        MemoryChunk * chunk = new MemoryChunk;
        const char * userfilename = table_info->m_user_filename;

        gchar * chunkfilename = g_build_filename(context->m_user_dir,
                                                 userfilename, NULL);

	/* check bin file exists. if not, create a new one. */
        if (chunk->load(chunkfilename)) {
            context->m_phrase_index->load(index, chunk);
        } else {
            delete chunk;
            context->m_phrase_index->create_sub_phrase(index);
        }

        g_free(chunkfilename);
        return true;
    }

    return false;
}

bool zhuyin_unload_phrase_library(zhuyin_context_t * context,
                                  guint8 index){
    /* tsi.bin can't be unloaded. */
    if (TSI_DICTIONARY == index)
        return false;

    assert(index < PHRASE_INDEX_LIBRARY_COUNT);

    context->m_phrase_index->unload(index);
    return true;
}

import_iterator_t * zhuyin_begin_add_phrases(zhuyin_context_t * context,
                                             guint8 index){
    import_iterator_t * iter = new import_iterator_t;
    iter->m_context = context;
    iter->m_phrase_index = index;
    return iter;
}

bool zhuyin_iterator_add_phrase(import_iterator_t * iter,
                                const char * phrase,
                                const char * pinyin,
                                gint count){
    /* if -1 == count, use the default value. */
    const gint default_count = 5;
    const guint32 unigram_factor = 3;
    if (-1 == count)
        count = default_count;

    zhuyin_context_t * & context = iter->m_context;
    FacadePhraseTable2 * & phrase_table = context->m_phrase_table;
    FacadeChewingTable * & pinyin_table = context->m_pinyin_table;
    FacadePhraseIndex * & phrase_index = context->m_phrase_index;

    bool result = false;

    if (NULL == phrase || NULL == pinyin)
        return result;

    /* check whether the phrase exists in phrase table */
    glong len_phrase = 0;
    ucs4_t * ucs4_phrase = g_utf8_to_ucs4(phrase, -1, NULL, &len_phrase, NULL);

    /* pinyin_option_t options = PINYIN_CORRECT_ALL | USE_TONE; */
    pinyin_option_t options = USE_TONE;
    ChewingDirectParser2 parser;
    ChewingKeyVector keys =
        g_array_new(FALSE, FALSE, sizeof(ChewingKey));
    ChewingKeyRestVector key_rests =
        g_array_new(FALSE, FALSE, sizeof(ChewingKeyRest));

    /* parse the pinyin. */
    parser.parse(options, keys, key_rests, pinyin, strlen(pinyin));

    if (len_phrase != keys->len)
        return result;

    if (0 == len_phrase || len_phrase >= MAX_PHRASE_LENGTH)
        return result;

    phrase_token_t token = null_token;
    GArray * tokenarray = g_array_new(FALSE, FALSE, sizeof(phrase_token_t));

    /* do phrase table search. */
    PhraseTokens tokens;
    memset(tokens, 0, sizeof(PhraseTokens));
    phrase_index->prepare_tokens(tokens);
    int retval = phrase_table->search(len_phrase, ucs4_phrase, tokens);
    int num = reduce_tokens(tokens, tokenarray);
    phrase_index->destroy_tokens(tokens);

    /* find the best token candidate. */
    for (size_t i = 0; i < tokenarray->len; ++i) {
        phrase_token_t candidate = g_array_index(tokenarray, phrase_token_t, i);
        if (null_token == token) {
            token = candidate;
            continue;
        }

        if (PHRASE_INDEX_LIBRARY_INDEX(candidate) == iter->m_phrase_index) {
            /* only one phrase string per sub phrase index. */
            assert(PHRASE_INDEX_LIBRARY_INDEX(token) != iter->m_phrase_index);
            token = candidate;
            continue;
        }
    }
    g_array_free(tokenarray, TRUE);

    PhraseItem item;
    /* check whether it exists in the same sub phrase index; */
    if (null_token != token &&
        PHRASE_INDEX_LIBRARY_INDEX(token) == iter->m_phrase_index) {
        /* if so, remove the phrase, add the pinyin for the phrase item,
           then add it back;*/
        phrase_index->get_phrase_item(token, item);
        assert(len_phrase == item.get_phrase_length());
        ucs4_t tmp_phrase[MAX_PHRASE_LENGTH];
        item.get_phrase_string(tmp_phrase);
        assert(0 == memcmp
               (ucs4_phrase, tmp_phrase, sizeof(ucs4_t) * len_phrase));

        PhraseItem * removed_item = NULL;
        retval = phrase_index->remove_phrase_item(token, removed_item);
        if (ERROR_OK == retval) {
            /* maybe check whether there are duplicated pronunciations here. */
            removed_item->add_pronunciation((ChewingKey *)keys->data,
                                            count);
            phrase_index->add_phrase_item(token, removed_item);
            delete removed_item;
            result = true;
        }
    } else {
        /* if not exists in the same sub phrase index,
           get the maximum token,
           then add it directly with maximum token + 1; */
        PhraseIndexRange range;
        retval = phrase_index->get_range(iter->m_phrase_index, range);

        if (ERROR_OK == retval) {
            token = range.m_range_end;
            if (0x00000000 == (token & PHRASE_MASK))
                token++;

            if (len_phrase == keys->len) { /* valid pinyin */
                phrase_table->add_index(len_phrase, ucs4_phrase, token);
                pinyin_table->add_index
                    (keys->len, (ChewingKey *)(keys->data), token);

                item.set_phrase_string(len_phrase, ucs4_phrase);
                item.add_pronunciation((ChewingKey *)(keys->data), count);
                phrase_index->add_phrase_item(token, &item);
                phrase_index->add_unigram_frequency(token,
                                                    count * unigram_factor);
                result = true;
            }
        }
    }

    g_array_free(key_rests, TRUE);
    g_array_free(keys, TRUE);
    g_free(ucs4_phrase);
    return result;
}

void zhuyin_end_add_phrases(import_iterator_t * iter){
    /* compact the content memory chunk of phrase index. */
    iter->m_context->m_phrase_index->compact();
    iter->m_context->m_modified = true;
    delete iter;
}

bool zhuyin_save(zhuyin_context_t * context){
    if (!context->m_user_dir)
        return false;

    if (!context->m_modified)
        return false;

    context->m_phrase_index->compact();

    const pinyin_table_info_t * phrase_files =
        context->m_system_table_info.get_table_info();

    /* skip the reserved zero phrase library. */
    for (size_t i = 1; i < PHRASE_INDEX_LIBRARY_COUNT; ++i) {
        PhraseIndexRange range;
        int retval = context->m_phrase_index->get_range(i, range);

        if (ERROR_NO_SUB_PHRASE_INDEX == retval)
            continue;

        const pinyin_table_info_t * table_info = phrase_files + i;

        if (NOT_USED == table_info->m_file_type)
            continue;

        const char * userfilename = table_info->m_user_filename;

        if (NULL == userfilename)
            continue;

        if (SYSTEM_FILE == table_info->m_file_type ||
            DICTIONARY == table_info->m_file_type) {
            /* system phrase library */
            MemoryChunk * chunk = new MemoryChunk;
            MemoryChunk * log = new MemoryChunk;
            const char * systemfilename = table_info->m_system_filename;

            /* check bin file in system dir. */
            gchar * chunkfilename = g_build_filename(context->m_system_dir,
                                                     systemfilename, NULL);
            chunk->load(chunkfilename);
            g_free(chunkfilename);
            context->m_phrase_index->diff(i, chunk, log);

            const char * userfilename = table_info->m_user_filename;
            gchar * tmpfilename = g_strdup_printf("%s.tmp", userfilename);

            gchar * tmppathname = g_build_filename(context->m_user_dir,
                                                   tmpfilename, NULL);
            g_free(tmpfilename);

            gchar * chunkpathname = g_build_filename(context->m_user_dir,
                                                     userfilename, NULL);
            log->save(tmppathname);

            int result = rename(tmppathname, chunkpathname);
            if (0 != result)
                fprintf(stderr, "rename %s to %s failed.\n",
                        tmppathname, chunkpathname);

            g_free(chunkpathname);
            g_free(tmppathname);
            delete log;
        }

        if (USER_FILE == table_info->m_file_type) {
            /* user phrase library */
            MemoryChunk * chunk = new MemoryChunk;
            context->m_phrase_index->store(i, chunk);

            const char * userfilename = table_info->m_user_filename;
            gchar * tmpfilename = g_strdup_printf("%s.tmp", userfilename);
            gchar * tmppathname = g_build_filename(context->m_user_dir,
                                                   tmpfilename, NULL);
            g_free(tmpfilename);

            gchar * chunkpathname = g_build_filename(context->m_user_dir,
                                                     userfilename, NULL);

            chunk->save(tmppathname);

            int result = rename(tmppathname, chunkpathname);
            if (0 != result)
                fprintf(stderr, "rename %s to %s failed.\n",
                        tmppathname, chunkpathname);

            g_free(chunkpathname);
            g_free(tmppathname);
            delete chunk;
        }
    }

    /* save user pinyin table */
    gchar * tmpfilename = g_build_filename
        (context->m_user_dir, USER_PINYIN_INDEX ".tmp", NULL);
    unlink(tmpfilename);
    gchar * filename = g_build_filename
        (context->m_user_dir, USER_PINYIN_INDEX, NULL);

    MemoryChunk * chunk = new MemoryChunk;
    context->m_pinyin_table->store(chunk);
    chunk->save(tmpfilename);
    delete chunk;

    int result = rename(tmpfilename, filename);
    if (0 != result)
        fprintf(stderr, "rename %s to %s failed.\n",
                tmpfilename, filename);

    g_free(tmpfilename);
    g_free(filename);

    /* save user phrase table */
    tmpfilename = g_build_filename
        (context->m_user_dir, USER_PHRASE_INDEX ".tmp", NULL);
    unlink(tmpfilename);
    filename = g_build_filename
        (context->m_user_dir, USER_PHRASE_INDEX, NULL);

    chunk = new MemoryChunk;
    context->m_phrase_table->store(chunk);
    chunk->save(tmpfilename);
    delete chunk;

    result = rename(tmpfilename, filename);
    if (0 != result)
        fprintf(stderr, "rename %s to %s failed.\n",
                tmpfilename, filename);

    g_free(tmpfilename);
    g_free(filename);

    /* save user bi-gram */
    tmpfilename = g_build_filename
        (context->m_user_dir, USER_BIGRAM ".tmp", NULL);
    unlink(tmpfilename);
    filename = g_build_filename(context->m_user_dir, USER_BIGRAM, NULL);
    context->m_user_bigram->save_db(tmpfilename);

    result = rename(tmpfilename, filename);
    if (0 != result)
        fprintf(stderr, "rename %s to %s failed.\n",
                tmpfilename, filename);

    g_free(tmpfilename);
    g_free(filename);

    mark_version(context);

    context->m_modified = false;
    return true;
}

bool zhuyin_set_chewing_scheme(zhuyin_context_t * context,
                               ZhuyinScheme scheme){
    delete context->m_chewing_parser;
    context->m_chewing_parser = NULL;

    switch(scheme) {
    case CHEWING_STANDARD:
    case CHEWING_IBM:
    case CHEWING_GINYIEH:
    case CHEWING_ETEN:
    case CHEWING_STANDARD_DVORAK: {
        ChewingSimpleParser2 * parser = new ChewingSimpleParser2();
        parser->set_scheme(scheme);
        context->m_chewing_parser = parser;
        break;
    }
    case CHEWING_HSU:
    case CHEWING_ETEN26:
    case CHEWING_HSU_DVORAK: {
        ChewingDiscreteParser2 * parser = new ChewingDiscreteParser2();
        parser->set_scheme(scheme);
        context->m_chewing_parser = parser;
        break;
    }
    case CHEWING_DACHEN_CP26:
        context->m_chewing_parser = new ChewingDaChenCP26Parser2();
        break;
    default:
        assert(FALSE);
    }
    return true;
}

bool zhuyin_set_full_pinyin_scheme(zhuyin_context_t * context,
                                   ZhuyinScheme scheme){
    context->m_full_pinyin_scheme = scheme;
    context->m_full_pinyin_parser->set_scheme(scheme);
    return true;
}

void zhuyin_fini(zhuyin_context_t * context){
    delete context->m_full_pinyin_parser;
    delete context->m_chewing_parser;
    delete context->m_pinyin_table;
    delete context->m_phrase_table;
    delete context->m_phrase_index;
    delete context->m_system_bigram;
    delete context->m_user_bigram;
    delete context->m_pinyin_lookup;
    delete context->m_phrase_lookup;

    g_free(context->m_system_dir);
    g_free(context->m_user_dir);
    context->m_modified = false;

    delete context;
}

bool zhuyin_mask_out(zhuyin_context_t * context,
                     phrase_token_t mask,
                     phrase_token_t value) {

    context->m_pinyin_table->mask_out(mask, value);
    context->m_phrase_table->mask_out(mask, value);
    context->m_user_bigram->mask_out(mask, value);

    const pinyin_table_info_t * phrase_files =
        context->m_system_table_info.get_table_info();

    /* mask out the phrase index. */
    for (size_t index = 1; index < PHRASE_INDEX_LIBRARY_COUNT; ++index) {
        PhraseIndexRange range;
        int retval = context->m_phrase_index->get_range(index, range);

        if (ERROR_NO_SUB_PHRASE_INDEX == retval)
            continue;

        const pinyin_table_info_t * table_info = phrase_files + index;

        if (NOT_USED == table_info->m_file_type)
            continue;

        const char * userfilename = table_info->m_user_filename;

        if (NULL == userfilename)
            continue;

        if (SYSTEM_FILE == table_info->m_file_type ||
            DICTIONARY == table_info->m_file_type) {
            /* system phrase library */
            MemoryChunk * chunk = new MemoryChunk;

            const char * systemfilename = table_info->m_system_filename;
            /* check bin file in system dir. */
            gchar * chunkfilename = g_build_filename(context->m_system_dir,
                                                     systemfilename, NULL);
            chunk->load(chunkfilename);
            g_free(chunkfilename);

            context->m_phrase_index->load(index, chunk);

            const char * userfilename = table_info->m_user_filename;

            chunkfilename = g_build_filename(context->m_user_dir,
                                             userfilename, NULL);

            MemoryChunk * log = new MemoryChunk;
            log->load(chunkfilename);
            g_free(chunkfilename);

            /* merge the chunk log with mask. */
            context->m_phrase_index->merge_with_mask(index, log, mask, value);
        }

        if (USER_FILE == table_info->m_file_type) {
            /* user phrase library */
            context->m_phrase_index->mask_out(index, mask, value);
        }
    }

    context->m_phrase_index->compact();
    return true;
}

/* copy from options to context->m_options. */
bool zhuyin_set_options(zhuyin_context_t * context,
                        pinyin_option_t options){
    context->m_options = options;
    context->m_pinyin_table->set_options(context->m_options);
    context->m_pinyin_lookup->set_options(context->m_options);
    return true;
}


zhuyin_instance_t * zhuyin_alloc_instance(zhuyin_context_t * context){
    zhuyin_instance_t * instance = new zhuyin_instance_t;
    instance->m_context = context;

    instance->m_raw_user_input = NULL;

    instance->m_prefixes = g_array_new(FALSE, FALSE, sizeof(phrase_token_t));
    instance->m_pinyin_keys = g_array_new(FALSE, FALSE, sizeof(ChewingKey));
    instance->m_pinyin_key_rests =
        g_array_new(FALSE, FALSE, sizeof(ChewingKeyRest));

    instance->m_parsed_len = 0;

    instance->m_constraints = g_array_new
        (TRUE, FALSE, sizeof(lookup_constraint_t));
    instance->m_match_results =
        g_array_new(FALSE, FALSE, sizeof(phrase_token_t));
    instance->m_candidates =
        g_array_new(FALSE, FALSE, sizeof(lookup_candidate_t));

    return instance;
}

void zhuyin_free_instance(zhuyin_instance_t * instance){
    g_free(instance->m_raw_user_input);
    g_array_free(instance->m_prefixes, TRUE);
    g_array_free(instance->m_pinyin_keys, TRUE);
    g_array_free(instance->m_pinyin_key_rests, TRUE);
    g_array_free(instance->m_constraints, TRUE);
    g_array_free(instance->m_match_results, TRUE);
    g_array_free(instance->m_candidates, TRUE);

    delete instance;
}


static bool pinyin_update_constraints(zhuyin_instance_t * instance){
    zhuyin_context_t * & context = instance->m_context;
    ChewingKeyVector & pinyin_keys = instance->m_pinyin_keys;
    CandidateConstraints & constraints = instance->m_constraints;

    size_t key_len = constraints->len;
    g_array_set_size(constraints, pinyin_keys->len);
    for (size_t i = key_len; i < pinyin_keys->len; ++i ) {
        lookup_constraint_t * constraint =
            &g_array_index(constraints, lookup_constraint_t, i);
        constraint->m_type = NO_CONSTRAINT;
    }

    context->m_pinyin_lookup->validate_constraint
        (constraints, pinyin_keys);

    return true;
}


bool zhuyin_guess_sentence(zhuyin_instance_t * instance){
    zhuyin_context_t * & context = instance->m_context;

    g_array_set_size(instance->m_prefixes, 0);
    g_array_append_val(instance->m_prefixes, sentence_start);

    pinyin_update_constraints(instance);
    bool retval = context->m_pinyin_lookup->get_best_match
        (instance->m_prefixes,
         instance->m_pinyin_keys,
         instance->m_constraints,
         instance->m_match_results);

    return retval;
}

bool zhuyin_guess_sentence_with_prefix(zhuyin_instance_t * instance,
                                       const char * prefix){
    zhuyin_context_t * & context = instance->m_context;

    FacadePhraseIndex * & phrase_index = context->m_phrase_index;

    g_array_set_size(instance->m_prefixes, 0);
    g_array_append_val(instance->m_prefixes, sentence_start);

    glong len_str = 0;
    ucs4_t * ucs4_str = g_utf8_to_ucs4(prefix, -1, NULL, &len_str, NULL);
    GArray * tokenarray = g_array_new(FALSE, FALSE, sizeof(phrase_token_t));

    if (ucs4_str && len_str) {
        /* add prefixes. */
        for (ssize_t i = 1; i <= len_str; ++i) {
            if (i > MAX_PHRASE_LENGTH)
                break;

            ucs4_t * start = ucs4_str + len_str - i;

            PhraseTokens tokens;
            memset(tokens, 0, sizeof(tokens));
            phrase_index->prepare_tokens(tokens);
            int result = context->m_phrase_table->search(i, start, tokens);
            int num = reduce_tokens(tokens, tokenarray);
            phrase_index->destroy_tokens(tokens);

            if (result & SEARCH_OK)
                g_array_append_vals(instance->m_prefixes,
                                    tokenarray->data, tokenarray->len);
        }
    }
    g_array_free(tokenarray, TRUE);
    g_free(ucs4_str);

    pinyin_update_constraints(instance);
    bool retval = context->m_pinyin_lookup->get_best_match
        (instance->m_prefixes,
         instance->m_pinyin_keys,
         instance->m_constraints,
         instance->m_match_results);

    return retval;
}

bool zhuyin_phrase_segment(zhuyin_instance_t * instance,
                           const char * sentence){
    zhuyin_context_t * & context = instance->m_context;

    const glong num_of_chars = g_utf8_strlen(sentence, -1);
    glong ucs4_len = 0;
    ucs4_t * ucs4_str = g_utf8_to_ucs4(sentence, -1, NULL, &ucs4_len, NULL);

    g_return_val_if_fail(num_of_chars == ucs4_len, FALSE);

    bool retval = context->m_phrase_lookup->get_best_match
        (ucs4_len, ucs4_str, instance->m_match_results);

    g_free(ucs4_str);
    return retval;
}

/* the returned sentence should be freed by g_free(). */
bool zhuyin_get_sentence(zhuyin_instance_t * instance,
                         char ** sentence){
    zhuyin_context_t * & context = instance->m_context;

    bool retval = zhuyin::convert_to_utf8
        (context->m_phrase_index, instance->m_match_results,
         NULL, false, *sentence);

    return retval;
}

bool zhuyin_parse_full_pinyin(zhuyin_instance_t * instance,
                              const char * onepinyin,
                              ChewingKey * onekey){
    zhuyin_context_t * & context = instance->m_context;

    int pinyin_len = strlen(onepinyin);
    bool retval = context->m_full_pinyin_parser->parse_one_key
        ( context->m_options, *onekey, onepinyin, pinyin_len);
    return retval;
}

size_t zhuyin_parse_more_full_pinyins(zhuyin_instance_t * instance,
                                      const char * pinyins){
    zhuyin_context_t * & context = instance->m_context;

    g_free(instance->m_raw_user_input);
    instance->m_raw_user_input = g_strdup(pinyins);
    int pinyin_len = strlen(pinyins);

    int parsed_len = context->m_full_pinyin_parser->parse
        ( context->m_options, instance->m_pinyin_keys,
          instance->m_pinyin_key_rests, pinyins, pinyin_len);

    instance->m_parsed_len = parsed_len;
    return parsed_len;
}

bool zhuyin_parse_chewing(zhuyin_instance_t * instance,
                          const char * onechewing,
                          ChewingKey * onekey){
    zhuyin_context_t * & context = instance->m_context;

    int chewing_len = strlen(onechewing);
    bool retval = context->m_chewing_parser->parse_one_key
        ( context->m_options, *onekey, onechewing, chewing_len );
    return retval;
}

size_t zhuyin_parse_more_chewings(zhuyin_instance_t * instance,
                                  const char * chewings){
    zhuyin_context_t * & context = instance->m_context;

    g_free(instance->m_raw_user_input);
    instance->m_raw_user_input = g_strdup(chewings);
    int chewing_len = strlen(chewings);

    int parsed_len = context->m_chewing_parser->parse
        ( context->m_options, instance->m_pinyin_keys,
          instance->m_pinyin_key_rests, chewings, chewing_len);

    instance->m_parsed_len = parsed_len;
    return parsed_len;
}

size_t zhuyin_get_parsed_input_length(zhuyin_instance_t * instance) {
    return instance->m_parsed_len;
}

bool zhuyin_in_chewing_keyboard(zhuyin_instance_t * instance,
                                const char key, gchar *** symbols) {
    zhuyin_context_t * & context = instance->m_context;
    return context->m_chewing_parser->in_chewing_scheme
        (context->m_options, key, *symbols);
}

#if 0
static gint compare_item_with_token(gconstpointer lhs,
                                    gconstpointer rhs) {
    lookup_candidate_t * item_lhs = (lookup_candidate_t *)lhs;
    lookup_candidate_t * item_rhs = (lookup_candidate_t *)rhs;

    phrase_token_t token_lhs = item_lhs->m_token;
    phrase_token_t token_rhs = item_rhs->m_token;

    return (token_lhs - token_rhs);
}
#endif

static gint compare_item_with_frequency(gconstpointer lhs,
                                        gconstpointer rhs) {
    lookup_candidate_t * item_lhs = (lookup_candidate_t *)lhs;
    lookup_candidate_t * item_rhs = (lookup_candidate_t *)rhs;

    guint32 freq_lhs = item_lhs->m_freq;
    guint32 freq_rhs = item_rhs->m_freq;

    return -(freq_lhs - freq_rhs); /* in descendant order */
}

static phrase_token_t _get_previous_token(zhuyin_instance_t * instance,
                                          size_t offset) {
    phrase_token_t prev_token = null_token;
    ssize_t i;

    if (0 == offset) {
        /* get previous token from prefixes. */
        prev_token = sentence_start;
        size_t prev_token_len = 0;

        zhuyin_context_t * context = instance->m_context;
        TokenVector prefixes = instance->m_prefixes;
        PhraseItem item;

        for (size_t i = 0; i < prefixes->len; ++i) {
            phrase_token_t token = g_array_index(prefixes, phrase_token_t, i);
            if (sentence_start == token)
                continue;

            int retval = context->m_phrase_index->get_phrase_item(token, item);
            if (ERROR_OK == retval) {
                size_t token_len = item.get_phrase_length();
                if (token_len > prev_token_len) {
                    /* found longer match, and save it. */
                    prev_token = token;
                    prev_token_len = token_len;
                }
            }
        }
    } else {
        /* get previous token from match results. */
        assert (0 < offset);

        phrase_token_t cur_token = g_array_index
            (instance->m_match_results, phrase_token_t, offset);
        if (null_token != cur_token) {
            for (i = offset - 1; i >= 0; --i) {
                cur_token = g_array_index
                    (instance->m_match_results, phrase_token_t, i);
                if (null_token != cur_token) {
                    prev_token = cur_token;
                    break;
                }
            }
        }
    }

    return prev_token;
}

static void _append_items(zhuyin_context_t * context,
                          PhraseIndexRanges ranges,
                          lookup_candidate_t * template_item,
                          CandidateVector items) {
    /* reduce and append to a single GArray. */
    for (size_t m = 0; m < PHRASE_INDEX_LIBRARY_COUNT; ++m) {
        if (NULL == ranges[m])
            continue;

        for (size_t n = 0; n < ranges[m]->len; ++n) {
            PhraseIndexRange * range =
                &g_array_index(ranges[m], PhraseIndexRange, n);
            for (size_t k = range->m_range_begin;
                 k < range->m_range_end; ++k) {
                lookup_candidate_t item;
                item.m_candidate_type = template_item->m_candidate_type;
                item.m_token = k;
                item.m_orig_rest = template_item->m_orig_rest;
                item.m_new_pinyins = g_strdup(template_item->m_new_pinyins);
                item.m_freq = template_item->m_freq;
                g_array_append_val(items, item);
            }
        }
    }
}

#if 0
static void _remove_duplicated_items(CandidateVector items) {
    /* remove the duplicated items. */
    phrase_token_t last_token = null_token, saved_token;
    for (size_t n = 0; n < items->len; ++n) {
        lookup_candidate_t * item = &g_array_index
            (items, lookup_candidate_t, n);

        saved_token = item->m_token;
        if (last_token == saved_token) {
            g_array_remove_index(items, n);
            n--;
        }
        last_token = saved_token;
    }
}
#endif

static void _compute_frequency_of_items(zhuyin_context_t * context,
                                        phrase_token_t prev_token,
                                        SingleGram * merged_gram,
                                        CandidateVector items) {
    pinyin_option_t & options = context->m_options;
    ssize_t i;

    PhraseItem cached_item;
    /* compute all freqs. */
    for (i = 0; i < items->len; ++i) {
        lookup_candidate_t * item = &g_array_index
            (items, lookup_candidate_t, i);
        phrase_token_t & token = item->m_token;

        gfloat bigram_poss = 0; guint32 total_freq = 0;
        if (options & DYNAMIC_ADJUST) {
            if (null_token != prev_token) {
                guint32 bigram_freq = 0;
                merged_gram->get_total_freq(total_freq);
                merged_gram->get_freq(token, bigram_freq);
                if (0 != total_freq)
                    bigram_poss = bigram_freq / (gfloat)total_freq;
            }
        }

        /* compute the m_freq. */
        FacadePhraseIndex * & phrase_index = context->m_phrase_index;
        phrase_index->get_phrase_item(token, cached_item);
        total_freq = phrase_index->get_phrase_index_total_freq();
        assert (0 < total_freq);

        gfloat lambda = context->m_system_table_info.get_lambda();

        /* Note: possibility value <= 1.0. */
        guint32 freq = (lambda * bigram_poss +
                        (1 - lambda) *
                        cached_item.get_unigram_frequency() /
                        (gfloat) total_freq) * 256 * 256 * 256;
        item->m_freq = freq;
    }
}

static bool _prepend_sentence_candidate(zhuyin_instance_t * instance,
                                        CandidateVector candidates) {
    /* check whether the best match candidate exists. */
    gchar * sentence = NULL;
    zhuyin_get_sentence(instance, &sentence);
    if (NULL == sentence)
        return false;
    g_free(sentence);

    /* prepend best match candidate to candidates. */
    lookup_candidate_t candidate;
    candidate.m_candidate_type = BEST_MATCH_CANDIDATE;
    g_array_prepend_val(candidates, candidate);

    return true;
}

static bool _compute_phrase_strings_of_items(zhuyin_instance_t * instance,
                                             size_t offset,
                                             bool is_after_cursor,
                                             CandidateVector candidates) {
    /* populate m_phrase_string in lookup_candidate_t. */

    for(size_t i = 0; i < candidates->len; ++i) {
        lookup_candidate_t * candidate = &g_array_index
            (candidates, lookup_candidate_t, i);

        switch(candidate->m_candidate_type) {
        case BEST_MATCH_CANDIDATE: {
            gchar * sentence = NULL;
            zhuyin_get_sentence(instance, &sentence);
            if (is_after_cursor) {
                candidate->m_phrase_string = g_strdup
                    (g_utf8_offset_to_pointer(sentence, offset));
            } else {
                candidate->m_phrase_string = g_utf8_substring
                    (sentence, 0, offset);
            }
            g_free(sentence);
            break;
        }
        case NORMAL_CANDIDATE_AFTER_CURSOR:
        case NORMAL_CANDIDATE_BEFORE_CURSOR:
            zhuyin_token_get_phrase
                (instance, candidate->m_token, NULL,
                 &(candidate->m_phrase_string));
            break;
        case ZOMBIE_CANDIDATE:
            break;
        }
    }

    return true;
}

static gint compare_indexed_item_with_phrase_string(gconstpointer lhs,
                                                    gconstpointer rhs,
                                                    gpointer userdata) {
    size_t index_lhs = *((size_t *) lhs);
    size_t index_rhs = *((size_t *) rhs);
    CandidateVector candidates = (CandidateVector) userdata;

    lookup_candidate_t * candidate_lhs =
        &g_array_index(candidates, lookup_candidate_t, index_lhs);
    lookup_candidate_t * candidate_rhs =
        &g_array_index(candidates, lookup_candidate_t, index_rhs);

    return -strcmp(candidate_lhs->m_phrase_string,
                   candidate_rhs->m_phrase_string); /* in descendant order */
}


static bool _remove_duplicated_items_by_phrase_string
(zhuyin_instance_t * instance,
 CandidateVector candidates) {
    size_t i;
    /* create the GArray of indexed item */
    GArray * indices = g_array_new(FALSE, FALSE, sizeof(size_t));
    for (i = 0; i < candidates->len; ++i)
        g_array_append_val(indices, i);

    /* sort the indices array by phrase array */
    g_array_sort_with_data
        (indices, compare_indexed_item_with_phrase_string, candidates);

    /* mark duplicated items as zombie candidate */
    lookup_candidate_t * cur_item, * saved_item = NULL;
    for (i = 0; i < indices->len; ++i) {
        size_t cur_index = g_array_index(indices, size_t, i);
        cur_item = &g_array_index(candidates, lookup_candidate_t, cur_index);

        /* handle the first candidate */
        if (NULL == saved_item) {
            saved_item = cur_item;
            continue;
        }

        if (0 == strcmp(saved_item->m_phrase_string,
                        cur_item->m_phrase_string)) {
            /* found duplicated candidates */

            /* keep best match candidate */
            if (BEST_MATCH_CANDIDATE == saved_item->m_candidate_type) {
                cur_item->m_candidate_type = ZOMBIE_CANDIDATE;
                continue;
            }

            if (BEST_MATCH_CANDIDATE == cur_item->m_candidate_type) {
                saved_item->m_candidate_type = ZOMBIE_CANDIDATE;
                saved_item = cur_item;
                continue;
            }

            /* keep the higher possiblity one
               to quickly move the word forward in the candidate list */
            if (cur_item->m_freq > saved_item->m_freq) {
                /* find better candidate */
                saved_item->m_candidate_type = ZOMBIE_CANDIDATE;
                saved_item = cur_item;
                continue;
            } else {
                cur_item->m_candidate_type = ZOMBIE_CANDIDATE;
                continue;
            }
        } else {
            /* keep the current candidate */
            saved_item = cur_item;
        }
    }

    g_array_free(indices, TRUE);

    /* remove zombie candidate from the returned candidates */
    for (i = 0; i < candidates->len; ++i) {
        lookup_candidate_t * candidate = &g_array_index
            (candidates, lookup_candidate_t, i);

        if (ZOMBIE_CANDIDATE == candidate->m_candidate_type) {
            g_free(candidate->m_phrase_string);
            g_free(candidate->m_new_pinyins);
            g_array_remove_index(candidates, i);
            i--;
        }
    }

    return true;
}

static bool _free_candidates(CandidateVector candidates) {
    /* free candidates */
    for (size_t i = 0; i < candidates->len; ++i) {
        lookup_candidate_t * candidate = &g_array_index
            (candidates, lookup_candidate_t, i);
        g_free(candidate->m_phrase_string);
        g_free(candidate->m_new_pinyins);
    }
    g_array_set_size(candidates, 0);

    return true;
}

bool zhuyin_guess_candidates_after_cursor(zhuyin_instance_t * instance,
                                          size_t offset) {

    zhuyin_context_t * & context = instance->m_context;
    pinyin_option_t & options = context->m_options;
    ChewingKeyVector & pinyin_keys = instance->m_pinyin_keys;

    _free_candidates(instance->m_candidates);

    size_t pinyin_len = pinyin_keys->len - offset;
    ssize_t i;

    /* lookup the previous token here. */
    phrase_token_t prev_token = null_token;

    if (options & DYNAMIC_ADJUST) {
        prev_token = _get_previous_token(instance, offset);
    }

    SingleGram merged_gram;
    SingleGram * system_gram = NULL, * user_gram = NULL;

    if (options & DYNAMIC_ADJUST) {
        if (null_token != prev_token) {
            context->m_system_bigram->load(prev_token, system_gram);
            context->m_user_bigram->load(prev_token, user_gram);
            merge_single_gram(&merged_gram, system_gram, user_gram);
        }
    }

    PhraseIndexRanges ranges;
    memset(ranges, 0, sizeof(ranges));
    context->m_phrase_index->prepare_ranges(ranges);

    GArray * items = g_array_new(FALSE, FALSE, sizeof(lookup_candidate_t));

    for (i = pinyin_len; i >= 1; --i) {
        g_array_set_size(items, 0);

        ChewingKey * keys = &g_array_index
            (pinyin_keys, ChewingKey, offset);

        /* do pinyin search. */
        int retval = context->m_pinyin_table->search
            (i, keys, ranges);

        if ( !(retval & SEARCH_OK) )
            continue;

        lookup_candidate_t template_item;
        _append_items(context, ranges, &template_item, items);

#if 0
        g_array_sort(items, compare_item_with_token);

        _remove_duplicated_items(items);
#endif

        _compute_frequency_of_items(context, prev_token, &merged_gram, items);

        /* sort the candidates of the same length by frequency. */
        g_array_sort(items, compare_item_with_frequency);

        /* transfer back items to tokens, and save it into candidates */
        for (size_t k = 0; k < items->len; ++k) {
            lookup_candidate_t * item = &g_array_index
                (items, lookup_candidate_t, k);
            g_array_append_val(instance->m_candidates, *item);
        }

#if 0
        if (!(retval & SEARCH_CONTINUED))
            break;
#endif
    }

    g_array_free(items, TRUE);
    context->m_phrase_index->destroy_ranges(ranges);
    if (system_gram)
        delete system_gram;
    if (user_gram)
        delete user_gram;

    /* post process to remove duplicated candidates */

    _prepend_sentence_candidate(instance, instance->m_candidates);

    _compute_phrase_strings_of_items(instance, offset,
                                     true, instance->m_candidates);

    _remove_duplicated_items_by_phrase_string(instance, instance->m_candidates);

    return true;
}

bool zhuyin_guess_candidates_before_cursor(zhuyin_instance_t * instance,
                                           size_t offset){

    zhuyin_context_t * & context = instance->m_context;
    pinyin_option_t & options = context->m_options;
    ChewingKeyVector & pinyin_keys = instance->m_pinyin_keys;

    _free_candidates(instance->m_candidates);

    size_t pinyin_len = offset;
    ssize_t i;

    PhraseIndexRanges ranges;
    memset(ranges, 0, sizeof(ranges));
    context->m_phrase_index->prepare_ranges(ranges);

    GArray * items = g_array_new(FALSE, FALSE, sizeof(lookup_candidate_t));

    for (i = pinyin_len; i >= 1; --i) {
        g_array_set_size(items, 0);

        /* lookup the previous token here. */
        phrase_token_t prev_token = null_token;

        if (options & DYNAMIC_ADJUST) {
            prev_token = _get_previous_token(instance, offset - i);
        }

        SingleGram merged_gram;
        SingleGram * system_gram = NULL, * user_gram = NULL;

        if (options & DYNAMIC_ADJUST) {
            if (null_token != prev_token) {
                context->m_system_bigram->load(prev_token, system_gram);
                context->m_user_bigram->load(prev_token, user_gram);
                merge_single_gram(&merged_gram, system_gram, user_gram);
            }
        }

        ChewingKey * keys = &g_array_index
            (pinyin_keys, ChewingKey, offset - i);

        /* do pinyin search. */
        int retval = context->m_pinyin_table->search
            (i, keys, ranges);

        if ( !(retval & SEARCH_OK) )
            continue;

        lookup_candidate_t template_item;
        template_item.m_candidate_type = NORMAL_CANDIDATE_BEFORE_CURSOR;
        _append_items(context, ranges, &template_item, items);

#if 0
        g_array_sort(items, compare_item_with_token);

        _remove_duplicated_items(items);
#endif

        _compute_frequency_of_items(context, prev_token, &merged_gram, items);

        /* sort the candidates of the same length by frequency. */
        g_array_sort(items, compare_item_with_frequency);

        /* transfer back items to tokens, and save it into candidates */
        for (size_t k = 0; k < items->len; ++k) {
            lookup_candidate_t * item = &g_array_index
                (items, lookup_candidate_t, k);
            g_array_append_val(instance->m_candidates, *item);
        }

#if 0
        if (!(retval & SEARCH_CONTINUED))
            break;
#endif

        if (system_gram)
            delete system_gram;
        if (user_gram)
            delete user_gram;
    }

    g_array_free(items, TRUE);
    context->m_phrase_index->destroy_ranges(ranges);

    /* post process to remove duplicated candidates */

    _prepend_sentence_candidate(instance, instance->m_candidates);

    _compute_phrase_strings_of_items(instance, offset,
                                     false, instance->m_candidates);

    _remove_duplicated_items_by_phrase_string(instance, instance->m_candidates);

    return true;
}

int zhuyin_choose_candidate(zhuyin_instance_t * instance,
                            size_t offset,
                            lookup_candidate_t * candidate){
    zhuyin_context_t * & context = instance->m_context;

    if (BEST_MATCH_CANDIDATE == candidate->m_candidate_type)
        return instance->m_pinyin_keys->len;

    /* sync m_constraints to the length of m_pinyin_keys. */
    bool retval = context->m_pinyin_lookup->validate_constraint
        (instance->m_constraints, instance->m_pinyin_keys);

    guint8 len = 0;
    if (NORMAL_CANDIDATE_AFTER_CURSOR == candidate->m_candidate_type) {
        phrase_token_t token = candidate->m_token;
        len = context->m_pinyin_lookup->add_constraint
            (instance->m_constraints, offset, token);
        offset = offset + len;
    }

    if (NORMAL_CANDIDATE_BEFORE_CURSOR == candidate->m_candidate_type) {
        phrase_token_t token = candidate->m_token;
        PhraseItem item;
        context->m_phrase_index->get_phrase_item(token, item);
        guint8 phrase_len = item.get_phrase_length();
        len = context->m_pinyin_lookup->add_constraint
            (instance->m_constraints, offset - phrase_len, token);
        if (offset < instance->m_pinyin_keys->len)
            offset = offset + 1;
    }

    /* safe guard: validate the m_constraints again. */
    retval = context->m_pinyin_lookup->validate_constraint
        (instance->m_constraints, instance->m_pinyin_keys) && len;

    return offset;
}

bool zhuyin_clear_constraint(zhuyin_instance_t * instance,
                             size_t offset){
    zhuyin_context_t * & context = instance->m_context;

    bool retval = context->m_pinyin_lookup->clear_constraint
        (instance->m_constraints, offset);

    return retval;
}

bool zhuyin_lookup_tokens(zhuyin_instance_t * instance,
                          const char * phrase, GArray * tokenarray){
    zhuyin_context_t * & context = instance->m_context;
    FacadePhraseIndex * & phrase_index = context->m_phrase_index;

    glong ucs4_len = 0;
    ucs4_t * ucs4_phrase = g_utf8_to_ucs4(phrase, -1, NULL, &ucs4_len, NULL);

    PhraseTokens tokens;
    memset(tokens, 0, sizeof(PhraseTokens));
    phrase_index->prepare_tokens(tokens);
    int retval = context->m_phrase_table->search(ucs4_len, ucs4_phrase, tokens);
    int num = reduce_tokens(tokens, tokenarray);
    phrase_index->destroy_tokens(tokens);

    return SEARCH_OK & retval;
}

bool zhuyin_train(zhuyin_instance_t * instance){
    if (!instance->m_context->m_user_dir)
        return false;

    zhuyin_context_t * & context = instance->m_context;
    context->m_modified = true;

    bool retval = context->m_pinyin_lookup->train_result2
        (instance->m_pinyin_keys, instance->m_constraints,
         instance->m_match_results);

    return retval;
}

bool zhuyin_reset(zhuyin_instance_t * instance){
    g_free(instance->m_raw_user_input);
    instance->m_raw_user_input = NULL;
    instance->m_parsed_len = 0;

    g_array_set_size(instance->m_prefixes, 0);
    g_array_set_size(instance->m_pinyin_keys, 0);
    g_array_set_size(instance->m_pinyin_key_rests, 0);
    g_array_set_size(instance->m_constraints, 0);
    g_array_set_size(instance->m_match_results, 0);
    _free_candidates(instance->m_candidates);

    return true;
}

bool zhuyin_get_bopomofo_string(zhuyin_instance_t * instance,
                                ChewingKey * key,
                                gchar ** utf8_str) {
    *utf8_str = NULL;
    if (0 == key->get_table_index())
        return false;

    *utf8_str = key->get_bopomofo_string();
    return true;
}

bool zhuyin_get_pinyin_string(zhuyin_instance_t * instance,
                              ChewingKey * key,
                              gchar ** utf8_str) {
    zhuyin_context_t * context = instance->m_context;

    *utf8_str = NULL;
    if (0 == key->get_table_index())
        return false;

    *utf8_str = key->get_pinyin_string(context->m_full_pinyin_scheme);
    return true;
}

bool zhuyin_token_get_phrase(zhuyin_instance_t * instance,
                             phrase_token_t token,
                             guint * len,
                             gchar ** utf8_str) {
    zhuyin_context_t * & context = instance->m_context;
    PhraseItem item;
    ucs4_t buffer[MAX_PHRASE_LENGTH];

    int retval = context->m_phrase_index->get_phrase_item(token, item);
    if (ERROR_OK != retval)
        return false;

    item.get_phrase_string(buffer);
    guint length = item.get_phrase_length();
    if (len)
        *len = length;
    if (utf8_str)
        *utf8_str = g_ucs4_to_utf8(buffer, length, NULL, NULL, NULL);
    return true;
}

bool zhuyin_token_get_n_pronunciation(zhuyin_instance_t * instance,
                                      phrase_token_t token,
                                      guint * num){
    *num = 0;
    zhuyin_context_t * & context = instance->m_context;
    PhraseItem item;

    int retval = context->m_phrase_index->get_phrase_item(token, item);
    if (ERROR_OK != retval)
        return false;

    *num = item.get_n_pronunciation();
    return true;
}

bool zhuyin_token_get_nth_pronunciation(zhuyin_instance_t * instance,
                                        phrase_token_t token,
                                        guint nth,
                                        ChewingKeyVector keys){
    g_array_set_size(keys, 0);
    zhuyin_context_t * & context = instance->m_context;
    PhraseItem item;
    ChewingKey buffer[MAX_PHRASE_LENGTH];
    guint32 freq = 0;

    int retval = context->m_phrase_index->get_phrase_item(token, item);
    if (ERROR_OK != retval)
        return false;

    item.get_nth_pronunciation(nth, buffer, freq);
    guint8 len = item.get_phrase_length();
    g_array_append_vals(keys, buffer, len);
    return true;
}

bool zhuyin_token_get_unigram_frequency(zhuyin_instance_t * instance,
                                        phrase_token_t token,
                                        guint * freq) {
    *freq = 0;
    zhuyin_context_t * & context = instance->m_context;
    PhraseItem item;

    int retval = context->m_phrase_index->get_phrase_item(token, item);
    if (ERROR_OK != retval)
        return false;

    *freq = item.get_unigram_frequency();
    return true;
}

bool zhuyin_token_add_unigram_frequency(zhuyin_instance_t * instance,
                                        phrase_token_t token,
                                        guint delta){
    zhuyin_context_t * & context = instance->m_context;
    int retval = context->m_phrase_index->add_unigram_frequency
        (token, delta);
    return ERROR_OK == retval;
}

bool zhuyin_get_n_candidate(zhuyin_instance_t * instance,
                            guint * num) {
    *num = instance->m_candidates->len;
    return true;
}

bool zhuyin_get_candidate(zhuyin_instance_t * instance,
                          guint index,
                          lookup_candidate_t ** candidate) {
    CandidateVector & candidates = instance->m_candidates;

    *candidate = NULL;

    if (index >= candidates->len)
        return false;

    *candidate = &g_array_index(candidates, lookup_candidate_t, index);

    return true;
}

bool zhuyin_get_candidate_type(zhuyin_instance_t * instance,
                               lookup_candidate_t * candidate,
                               lookup_candidate_type_t * type) {
    *type = candidate->m_candidate_type;
    return true;
}

bool zhuyin_get_candidate_string(zhuyin_instance_t * instance,
                                 lookup_candidate_t * candidate,
                                 const gchar ** utf8_str) {
    *utf8_str = candidate->m_phrase_string;
    return true;
}

bool zhuyin_get_n_zhuyin(zhuyin_instance_t * instance,
                         guint * num) {
    *num = 0;

    if (instance->m_pinyin_keys->len !=
        instance->m_pinyin_key_rests->len)
        return false;

    *num = instance->m_pinyin_keys->len;
    return true;
}

bool zhuyin_get_zhuyin_key(zhuyin_instance_t * instance,
                           guint index,
                           ChewingKey ** key) {
    ChewingKeyVector & pinyin_keys = instance->m_pinyin_keys;

    *key = NULL;

    if (index >= pinyin_keys->len)
        return false;

    *key = &g_array_index(pinyin_keys, ChewingKey, index);

    return true;
}

bool zhuyin_get_zhuyin_key_rest(zhuyin_instance_t * instance,
                                guint index,
                                ChewingKeyRest ** key_rest) {
    ChewingKeyRestVector & pinyin_key_rests =
        instance->m_pinyin_key_rests;

    *key_rest = NULL;

    if (index >= pinyin_key_rests->len)
        return false;

    *key_rest = &g_array_index(pinyin_key_rests, ChewingKeyRest, index);

    return true;
}

bool zhuyin_get_zhuyin_key_rest_positions(zhuyin_instance_t * instance,
                                          ChewingKeyRest * key_rest,
                                          guint16 * begin, guint16 * end) {
    if (begin)
        *begin = key_rest->m_raw_begin;

    if (end)
        *end = key_rest->m_raw_end;

    return true;
}

bool zhuyin_get_zhuyin_key_rest_length(zhuyin_instance_t * instance,
                                       ChewingKeyRest * key_rest,
                                       guint16 * length) {
    *length = key_rest->length();
    return true;
}

bool zhuyin_get_zhuyin_key_rest_offset(zhuyin_instance_t * instance,
                                       guint16 cursor,
                                       guint16 * offset) {
    assert (cursor <= instance->m_parsed_len);

    *offset = 0;

    guint len = 0;
    assert (instance->m_pinyin_keys->len ==
            instance->m_pinyin_key_rests->len);
    len = instance->m_pinyin_key_rests->len;

    ChewingKeyRestVector & pinyin_key_rests =
        instance->m_pinyin_key_rests;

    guint inner_cursor = len;

    guint16 prev_end = 0, cur_end;
    for (size_t i = 0; i < len; ++i) {
        ChewingKeyRest *pos = NULL;
        pos = &g_array_index(pinyin_key_rests, ChewingKeyRest, i);
        cur_end = pos->m_raw_end;

        if (prev_end <= cursor && cursor < cur_end)
            inner_cursor = i;

        prev_end = cur_end;
    }

    assert (inner_cursor >= 0);
    *offset = inner_cursor;

    return true;
}

bool zhuyin_get_raw_user_input(zhuyin_instance_t * instance,
                               const gchar ** utf8_str) {
    *utf8_str = instance->m_raw_user_input;
    return true;
}

bool zhuyin_get_n_phrase(zhuyin_instance_t * instance,
                         guint * num) {
    *num = instance->m_match_results->len;
    return true;
}

bool zhuyin_get_phrase_token(zhuyin_instance_t * instance,
                             guint index,
                             phrase_token_t * token){
    MatchResults & match_results = instance->m_match_results;

    *token = null_token;

    if (index >= match_results->len)
        return false;

    *token = g_array_index(match_results, phrase_token_t, index);

    return true;
}


/**
 *  Note: prefix is the text before the pre-edit string.
 */
