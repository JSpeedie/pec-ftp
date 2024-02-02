#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "aes.h"
#include "enc.h"

#define  READSIZE 1024


// TODO: what's going on here?
void init_enc() {
    printf("Init aes");
    //initialize_aes_sbox(sbox, sboxinv);
}

/* Computes a^b mod c */
uint64_t sq_mp(uint64_t a, uint64_t b, uint64_t c) {
    uint64_t r;
    uint64_t y = 1;

    while (b > 0) {
        r = b % 2;

        if (r == 1) {
            y = (y * a) % c;
        }

        a = a * a % c;
        b = b / 2;
    }

    return y;
}

void to_column_order(uint8_t text[16]) {
    uint8_t temp[16];
    memcpy(temp, text, 16);
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            text[4*j + i] = temp[4*i + j];
        }
    }
}

void to_row_order(uint8_t text[16]) {
    uint8_t temp[16];
    memcpy(temp, text, 16);
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            text[4*i + j] = temp[4*j + i];
        }
    }
}

// TODO: is there any way we can avoid doing some jank "in_name is really 'cat
// [filepath]'" stuff here? It seems misleading and also clunky
int enc_file(char in_name[], char out_name[], uint32_t key[4]) {
    FILE *in;
    FILE *fp;
    size_t read_len;
    char readbuf[READSIZE+1];
    char writebuf[READSIZE+17];
    uint8_t text[16];
    uint8_t rkeys[11][16];
    uint8_t sbox[256];
    uint8_t sboxinv[256];
    int i;
    int p = 0;
    int padded = 0;
    
    initialize_aes_sbox(sbox, sboxinv);

    if ((in = fopen(in_name, "rb")) == NULL) {
        return -1;
    }
    
    if((fp = fopen(out_name, "w")) == NULL){
        return -1;
    }
    
    expkey(rkeys, key, sbox);
    
    while (0 != (read_len = fread(readbuf, 1, READSIZE, in)) ) {
        for (i = 0; i < read_len - (read_len % 16); i += 16) {
            memcpy(text, &(readbuf[i]), 16);
            to_column_order(text);
            encrypt(text, rkeys, sbox);
            memcpy(&(writebuf[i]), text, 16);
        }
        
        if (read_len < READSIZE) {
            if (read_len % 16 != 0) {
                memcpy(text, &(readbuf[i]), (read_len % 16));
                
                int padnum = 16 - (read_len % 16);
                for (int j = (read_len % 16); j < 16; j++) {
                    text[j] = padnum;
                    read_len++;
                }
                
                padded = 1;
                to_column_order(text);
                encrypt(text, rkeys, sbox);
                memcpy(&(writebuf[i]), text, 16);
            }
            fseek(fp, p, SEEK_SET);
            fwrite(writebuf, 1, read_len, fp);
            p = p + read_len;
            break; /* If this condition is met, either an error occured or we have reached EOF. Break for safety. */
        } else {
            fseek(fp, p, SEEK_SET);
            fwrite(writebuf, 1, read_len, fp);
            p = p + read_len;
        }
    }
    
    if (!padded) {
    	for (int j = 0; j < 16; j++) {
            text[j] = 16;
        }
        
        encrypt(text, rkeys, sbox);
        fseek(fp, p, SEEK_SET);
        fwrite(text, 1, 16, fp);
    }
    
    fclose(in);
    fclose(fp);
    
    return 0;
}

int dec_file(char in_name[], char out_name[], uint32_t key[4]) {
    FILE *in;
    FILE *fp;
    size_t read_len;
    size_t read_len_prev;
    char readbuf[READSIZE+1];
    char writebuf[READSIZE+1];
    uint8_t sbox[256];
    uint8_t sboxinv[256];
    uint8_t text[16];
    uint8_t rkeys[11][16];
    int i;
    int p = 0;
    int padrm = 0;
    int first = 1;
    
    initialize_aes_sbox(sbox, sboxinv);

	if ((in = fopen(in_name, "rb")) == NULL) {
		return -1;
	}
    
    if((fp = fopen(out_name, "w")) == NULL){
        return -1;
    }
    
    expkey(rkeys, key, sbox);
    
    while (0 != (read_len = fread(readbuf, 1, READSIZE, in)) ) {
        if (!first) {
            fseek(fp, p, SEEK_SET);
            fwrite(writebuf, 1, read_len_prev, fp);
            p = p + read_len_prev;
        } else {
            first = 0;
        }
        
        for (i = 0; i < read_len - (read_len % 16); i += 16) {
            memcpy(text, &(readbuf[i]), 16);
            decrypt(text, rkeys, sboxinv);
            to_row_order(text);
            memcpy(&(writebuf[i]), text, 16);
        }
        
        if (read_len < READSIZE) {
            if (read_len % 16 == 0) {
                int padnum = writebuf[i - 1];
                fwrite(writebuf, 1, read_len - padnum, fp);
                padrm = 1;
                break;
            } else {
                return -1; /* Input should always be a multiple of the block size. */
            }
        }
        
        read_len_prev = read_len;
    }
    
    if (!padrm) {
    	int padnum = writebuf[i - 1];
        fwrite(writebuf, 1, read_len - padnum, fp);
    }
    
    fclose(in);
    fclose(fp);
    
    return 0;
}
