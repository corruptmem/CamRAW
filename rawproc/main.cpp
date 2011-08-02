//
//  main.cpp
//  rawproc
//
//  Created by Cameron Harris on 02/08/2011.
//  Copyright 2011 Altis Partners. All rights reserved.
//

#include <iostream>
#include <fstream>

#include "jpeg.h"

struct Cr2Header {
    uint16_t byte_order;
    uint16_t magic_word;
    uint32_t tiff_offset;
    uint16_t cr2_magic_word;
    uint8_t cr2_major_ver;
    uint8_t cr2_minor_ver;
    uint32_t raw_ifd_offset;
    
public:
    bool is_valid_header() {
        if(byte_order!= 0x4949) 
            return false;
        
        if(magic_word!= 0x002a)
            return false;
        
        if(tiff_offset != 0x10)
            return false;
        
        if(cr2_magic_word != 0x5243)
            return false;
        
        if(cr2_major_ver!=0x2)
            return false;
        
        if(cr2_minor_ver != 0x0)
            return false;
        
        return true;
    }
};

struct Cr2IfdEntry {
    uint16_t tag_id;
    uint16_t tag_type;
    uint32_t number_of_val;
    uint32_t val;
};

struct Cr2IfdHeader {
    uint16_t entries_count;
};

struct Cr2IfdFooter {
    uint32_t next_ifd;
};

struct Cr2Slices {
    uint16_t num_first_strips;
    uint16_t first_strip_px;
    uint16_t last_strip_px;
};

class Cr2Parser {
private:
    const char* _path;
    
public:
    Cr2Parser(const char* path) {
        _path = path;
    }
    
    void Parse() {
        std::ifstream file(_path);
        
        if(!file.is_open()) {
            throw "Error: Could not open file!";
        }
        
        Cr2Header header;
        file.read((char*)&header, sizeof(Cr2Header));
        
        if(!header.is_valid_header()){
            throw "Error: Invalid header!";
        }
        
        file.seekg(header.raw_ifd_offset, std::ios_base::beg);
        
        Cr2IfdHeader ifd_header;
        file.read((char*)&ifd_header, sizeof(Cr2IfdHeader));
        
        uint32_t strip_offset;
        uint32_t strip_byte_counts;
        uint32_t slice_info_offset;
        
        for(int i = 0; i<ifd_header.entries_count; i++) {
            Cr2IfdEntry ifd_entry;
            file.read((char*)&ifd_entry, sizeof(Cr2IfdEntry));
            
            switch(ifd_entry.tag_id) {
                case 0x103:
                    if(ifd_entry.val!=6) throw "Err: Didn't understand compression type 6";
                    break;
                case 0x111:
                    strip_offset = ifd_entry.val;
                    break;
                case 0x117:
                    strip_byte_counts = ifd_entry.val;
                    break;
                case 0xc640:
                    slice_info_offset = ifd_entry.val;
            }
        }
        
        Cr2IfdFooter ifd_footer;
        file.read((char*)&ifd_footer, sizeof(Cr2IfdFooter));
        
        Cr2Slices slices;
        file.seekg(slice_info_offset, std::ios_base::beg);
        file.read((char*)&slices, sizeof(Cr2Slices));
        
        char* raw_buf = new char[strip_byte_counts];
        file.seekg(strip_offset, std::ios_base::beg);
        file.read(raw_buf, strip_byte_counts);
        
        std::cout << "!!!" << std::endl;
        ljpeg jp((unsigned char*)raw_buf, strip_byte_counts);
        jp.start(0);
        //----------
        
        int jwide = jp.jhactual.wide * jp.jhactual.clrs;
        ushort *rp;
        std::ofstream out("/tmp/out.pgm");
        std::cout<<jp.jhactual.wide<<std::endl;
        std::cout<<"First count: "<<slices.num_first_strips<<std::endl;
        std::cout<<"First strip px: "<<slices.first_strip_px<<std::endl;
        std::cout<<"Last strip px: "<<slices.last_strip_px<<std::endl;
        
        out<<"P5"<<std::endl;
        int w = 1728;
        int h = (jp.jhactual.high*jp.jhactual.wide)/w;
        int total = w*h;
        //out<<"1336"<<std::endl;
        out<<w<<std::endl;
        out<<h<<std::endl;
        //out<<"3516"<<std::endl;
        out<<"65535"<<std::endl;
        
        std::cout<<total<<std::endl;
        int i = 0;
        
        for(int jrow = 0; jrow<jp.jhactual.high; jrow++) {
            rp = jp.row(jrow);
        
            
            for(int px = 0; px<jp.jhactual.wide; px++) {
                unsigned char c1;
                unsigned char c2;
                
                c2 = (unsigned char)(rp[px] & 0xFF);
                c1 = (unsigned char)((rp[px] & 0xFF00)>>8);
                
                out.put(c1);
                out.put(c2);
                
                i++;
                if(i>total) {
                    break;
                }
            }
            
            if(i>total) {
                break;
            }
            
            //std::cout<<jrow<<std::endl;
        }
        
            
            
            //--------
        jp.end();
        /*jp.end();*/
        
        out.close();
        
        
        std::cout << "Done"  << std::endl;
    }
};

int main (int argc, const char * argv[])
{
    try {
        const char* file_path = "/Users/cameron/camraw/test.cr2";
        Cr2Parser parser(file_path);
        parser.Parse();
    } catch (const char* str) {
        std::cout<<str<<std::endl;
    }

    
    
    return 0;
}

