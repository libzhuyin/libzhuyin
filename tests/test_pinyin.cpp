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
#include <stdlib.h>
#include <string.h>

int main(int argc, char * argv[]){
    zhuyin_context_t * context =
        zhuyin_init("../data", "../data");

    pinyin_option_t options = DYNAMIC_ADJUST;
    zhuyin_set_options(context, options);

    zhuyin_instance_t * instance = zhuyin_alloc_instance(context);

    char * prefixbuf = NULL; size_t prefixsize = 0;
    char * linebuf = NULL; size_t linesize = 0;
    ssize_t read;

    while( TRUE ){
        fprintf(stdout, "prefix:");
        fflush(stdout);

        if ((read = getline(&prefixbuf, &prefixsize, stdin)) == -1)
            break;

        if ( '\n' == prefixbuf[strlen(prefixbuf) - 1] ) {
            prefixbuf[strlen(prefixbuf) - 1] = '\0';
        }

        fprintf(stdout, "pinyin:");
        fflush(stdout);

        if ((read = getline(&linebuf, &linesize, stdin)) == -1)
            break;

        if ( '\n' == linebuf[strlen(linebuf) - 1] ) {
            linebuf[strlen(linebuf) - 1] = '\0';
        }

        if ( strcmp ( linebuf, "quit" ) == 0)
            break;

        zhuyin_parse_more_full_pinyins(instance, linebuf);
        zhuyin_guess_sentence_with_prefix(instance, prefixbuf);
        zhuyin_guess_candidates_after_cursor(instance, 0);

        guint len = 0;
        zhuyin_get_n_candidate(instance, &len);
        for (size_t i = 0; i < len; ++i) {
            lookup_candidate_t * candidate = NULL;
            zhuyin_get_candidate(instance, i, &candidate);

            const char * word = NULL;
            zhuyin_get_candidate_string(instance, candidate, &word);

            printf("%s\t", word);
        }
        printf("\n");

        zhuyin_train(instance);
        zhuyin_reset(instance);
        zhuyin_save(context);
    }

    zhuyin_free_instance(instance);

    zhuyin_mask_out(context, 0x0, 0x0);
    zhuyin_save(context);
    zhuyin_fini(context);

    free(prefixbuf); free(linebuf);
    return 0;
}
