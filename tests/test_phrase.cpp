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

    zhuyin_instance_t * instance = zhuyin_alloc_instance(context);

    char* linebuf = NULL;
    size_t size = 0;
    ssize_t read;
    while( (read = getline(&linebuf, &size, stdin)) != -1 ){
        if ( '\n' == linebuf[strlen(linebuf) - 1] ) {
            linebuf[strlen(linebuf) - 1] = '\0';
        }

	if ( strcmp ( linebuf, "quit" ) == 0)
            break;

        zhuyin_phrase_segment(instance, linebuf);
        guint len = 0;
        zhuyin_get_n_phrase(instance, &len);

        for ( size_t i = 0; i < len; ++i ){
            phrase_token_t token = null_token;
            zhuyin_get_phrase_token(instance, i, &token);

            if ( null_token == token )
                continue;

            char * word = NULL;
            zhuyin_token_get_phrase(instance, token, NULL, &word);
            printf("%s\t", word);
            g_free(word);
        }
        printf("\n");

        zhuyin_save(context);
    }

    zhuyin_free_instance(instance);

    zhuyin_mask_out(context, 0x0, 0x0);
    zhuyin_save(context);
    zhuyin_fini(context);

    free(linebuf);
    return 0;
}
