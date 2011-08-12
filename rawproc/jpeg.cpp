#include <string.h>
#include <iostream>

#include "jpeg.h"



#define FORC(cnt) for (c=0; c < cnt; c++)
#define FORC3 FORC(3)
#define FORC4 FORC(4)
#define FORCC FORC(colors)



ljpeg::ljpeg(const unsigned char* data, int size) : SeekCur(1) {
    jh = &jhactual;
    _data = data;
    _size = size;
    _pos = 0;
}
    
    void ljpeg::fread(unsigned char* data, int m, int n) {
        int dst = 0;
        for(int i = 0; i<n; i++) {
            
            int cp = m;
            if(_pos + cp >= _size) {
                cp = _size - _pos;
            }
            
            memcpy(data+dst, _data+_pos, cp);
            _pos += cp;
            dst += cp;
            if(_pos > _size) return;
        }
    }
    
    unsigned int ljpeg::fgetc() {
        if(_pos >= _size) {
            return EOF;
        }
        
        return _data[_pos++];
    }
    
    unsigned int ljpeg::getc() {
        return fgetc();
    }
    
    void ljpeg::fseek(int amount, int seektype) {
        _pos = _pos + amount;
    }
    
    unsigned ljpeg::getbithuff (int nbits, ushort *huff)
    {
        static unsigned bitbuf=0;
        static int vbits=0, reset=0;
        unsigned c;
        
        if (nbits == -1)
        {
            bitbuf = 0;
            vbits = 0;
            reset = 0;
            
            return 0;
        }
        
        if (nbits == 0 || vbits < 0) {
            return 0;
        }
        
        while(true) {
            if(vbits >= nbits) break;
            
            c = fgetc();
            
            if(c == EOF) break;
            
            if(zero_after_ff && c == 0xff && fgetc()) {
                reset = 1;
                break;
            }
            
            reset = 0;
            
            bitbuf = (bitbuf << 8) + (uchar) c;
            vbits += 8;
        }
        
        c = bitbuf << (32-vbits) >> (32-nbits);
        if (huff) {
            vbits -= huff[c] >> 8;
            c = (uchar) huff[c];
        } else {
            vbits -= nbits;
        }
        
        if (vbits < 0) {
            throw "Vbits was 0";
        }
        
        return c;
    }
    
    unsigned ljpeg::getbits(int nbits) {
        return getbithuff(nbits, 0);
    }
    
    ushort * ljpeg::make_decoder_ref (const uchar **source)
    {
        int max, len, h, i, j;
        const uchar *count;
        ushort *huff;
        
        *source += 16;
        count = *source - 17;
        
        for (max=16; max && !count[max]; max--);
        
        uint32_t huff_size = 1+(1<<max);
        huff = new ushort[huff_size];
        memset(huff, 0, huff_size);
        
        huff[0] = max;
        for (h=len=1; len <= max; len++) {
            for (i=0; i < count[len]; i++, ++*source) {
                for (j=0; j < 1 << (max-len); j++) {
                    if (h <= 1 << max) {
                        huff[h++] = len << 8 | **source;
                    }
                }
            }
        }
        return huff;
    }
    
    int ljpeg::start (int info_only)
    {
        int c, tag, len;
        uchar data[0x10000];
        const uchar *dp;
        
                
        memset (jh, 0, sizeof *jh);
        jh->restart = INT_MAX;
        fread (data, 2, 1);
        if (data[1] != 0xd8) return 0;
        
        do {
            fread (data, 2, 2);
            
            tag =  data[0] << 8 | data[1];
            len = (data[2] << 8 | data[3]) - 2;
            if (tag <= 0xff00) return 0;
            fread (data, 1, len);
            switch (tag) {
                case 0xffc3:
                    jh->sraw = ((data[7] >> 4) * (data[7] & 15) - 1) & 3;
                case 0xffc0:
                    jh->bits = data[0];
                    jh->high = data[1] << 8 | data[2];
                    jh->wide = data[3] << 8 | data[4];
                    jh->clrs = data[5] + jh->sraw;
                    if (len == 9 && !dng_version) getc();
                    break;
                case 0xffc4:
                    if (info_only) break;
                    for (dp = data; dp < data+len && (c = *dp++) < 4; )
                        jh->free[c] = jh->huff[c] = make_decoder_ref (&dp);
                    break;
                case 0xffda:
                    jh->psv = data[1+data[0]*2];
                    jh->bits -= data[3+data[0]*2] & 15;
                    break;
                case 0xffdd:
                    jh->restart = data[0] << 8 | data[1];
            }
        } while (tag != 0xffda);
        if (info_only) return 1;
        FORC(5) if (!jh->huff[c+1]) jh->huff[c+1] = jh->huff[c];
        if (jh->sraw) {
            FORC(4)        jh->huff[2+c] = jh->huff[1];
            FORC(jh->sraw) jh->huff[1+c] = jh->huff[0];
        }
        jh->row = (ushort *) calloc (jh->wide*jh->clrs, 4);
        merror (jh->row, "ljpeg_start()");
        return zero_after_ff = 1;
    }
    
    void ljpeg::merror (void *ptr, const char *where)
    {
        if (ptr) return;
        throw "Out of memory";
    }

    void ljpeg::end ()
    {
        int c;
        FORC4 if (jh->free[c]) free (jh->free[c]);
        free (jh->row);
    }

    int ljpeg::diff (ushort *huff)
    {
        int len, diff;
        
        len = gethuff(huff);
        
        if (len == 16 && (!dng_version || dng_version >= 0x1010000))
            return -32768;
        diff = getbits(len);
        if ((diff & (1 << (len-1))) == 0)
            diff -= (1 << len) - 1;
        return diff;
    }

    ushort * ljpeg::row (int jrow)
    {
        int col, c, diff, pred, spred=0;
        ushort mark=0, *row[3];
    
        if (jrow * jh->wide % jh->restart == 0) {
            FORC(6) jh->vpred[c] = 1 << (jh->bits-1);
            if (jrow) {
                
                fseek (-2, SEEK_CUR);
                do mark = (mark << 8) + (c = fgetc());
                while (c != EOF && mark >> 4 != 0xffd);
            }
            getbits(-1);
        }
        
        FORC3 row[c] = jh->row + jh->wide*jh->clrs*((jrow+c) & 1);
        for (col=0; col < jh->wide; col++)
            FORC(jh->clrs) {
                diff = this->diff (jh->huff[c]);
                if (jh->sraw && c <= jh->sraw && (col | c)) {
                    pred = spred;
                } else if (col) {
                    pred = row[0][-jh->clrs];
                } else{
                    pred = (jh->vpred[c] += diff) - diff;
                }
                
                if (jrow && col) {
                    switch (jh->psv) {
                        case 1:	
                            break;
                            
                        case 2: 
                            pred = row[1][0];					
                            break;
                            
                        case 3: 
                            pred = row[1][-jh->clrs];
                            break;
                            
                        case 4: 
                            pred = pred + row[1][0] - row[1][-jh->clrs];
                            break;
                            
                        case 5: 
                            pred = pred + ((row[1][0] - row[1][-jh->clrs]) >> 1);
                            break;
                            
                        case 6:
                            pred = row[1][0] + ((pred - row[1][-jh->clrs]) >> 1);
                            break;
                            
                        case 7: 
                            pred = (pred + row[1][0]) >> 1;
                            break;
                            
                        default: 
                            pred = 0;
                    }
                }
                
                if ((**row = pred + diff) >> jh->bits) {
                    throw "Data error";
                }
                
                if (c <= jh->sraw) {
                    spred = **row;
                }
                
                row[0]++; 
                row[1]++;
            }
                    
        return row[2];
    }
