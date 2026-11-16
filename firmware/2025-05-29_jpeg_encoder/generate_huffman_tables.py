#!/usr/bin/env python3
"""
Huffman Table Precomputation Script for Tiny JPEG Encoder

This script precomputes the extended Huffman data structures and exports them
as C literals that can be included directly in the library, eliminating the
need for runtime computation.
"""

def get_code_lengths(bits):
    """Returns all code sizes from the BITS specification (JPEG C.3)"""
    huffsize = []
    for i in range(16):
        for j in range(bits[i]):
            huffsize.append(i + 1)
    huffsize.append(0)
    return huffsize

def get_codes(huffsize, count):
    """Fills out the prefixes for each code."""
    codes = []
    code = 0
    k = 0
    sz = huffsize[0]
    
    while True:
        while k < len(huffsize) and huffsize[k] == sz:
            if k >= count:
                break
            codes.append(code)
            code += 1
            k += 1
        
        if k >= len(huffsize) or huffsize[k] == 0 or k >= count:
            break
            
        while huffsize[k] != sz:
            code = code << 1
            sz += 1
            if sz > 16:  # Prevent overflow
                break
    
    return codes

def get_extended(huffval, huffsize, huffcode, count):
    """Generate extended huffman tables"""
    ehuffsize = [0] * 256
    ehuffcode = [0] * 256
    
    for k in range(count):
        if k < len(huffval):
            val = huffval[k]
            if val < 256:
                ehuffcode[val] = huffcode[k]
                ehuffsize[val] = huffsize[k]
    
    return ehuffsize, ehuffcode

def format_c_array(name, data, data_type="uint8_t", cols=16):
    """Format data as a C array literal"""
    result = f"static const {data_type} {name}[] = {{\n"
    
    for i in range(0, len(data), cols):
        row = data[i:i+cols]
        if data_type == "uint16_t":
            formatted_row = ", ".join(f"0x{x:04X}" for x in row)
        else:
            formatted_row = ", ".join(f"0x{x:02X}" if x > 9 else f"{x}" for x in row)
        result += f"    {formatted_row}"
        if i + cols < len(data):
            result += ","
        result += "\n"
    
    result += "};\n"
    return result

def main():
    # JPEG Standard Huffman Tables (from tiny_jpeg_packed.h)
    
    # LUMA DC
    tjei_default_ht_luma_dc_len = [0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0]
    tjei_default_ht_luma_dc = [0,1,2,3,4,5,6,7,8,9,10,11]
    
    # LUMA AC  
    tjei_default_ht_luma_ac_len = [0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d]
    tjei_default_ht_luma_ac = [
        0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
        0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
        0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
        0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
        0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
        0xF9, 0xFA
    ]
    
    # CHROMA DC
    tjei_default_ht_chroma_dc_len = [0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0]
    tjei_default_ht_chroma_dc = [0,1,2,3,4,5,6,7,8,9,10,11]
    
    # CHROMA AC
    tjei_default_ht_chroma_ac_len = [0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77]
    tjei_default_ht_chroma_ac = [
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
        0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
        0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
        0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
        0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
        0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
        0xF9, 0xFA
    ]
    
    # Table definitions
    tables = [
        ("LUMA_DC", tjei_default_ht_luma_dc_len, tjei_default_ht_luma_dc),
        ("LUMA_AC", tjei_default_ht_luma_ac_len, tjei_default_ht_luma_ac),
        ("CHROMA_DC", tjei_default_ht_chroma_dc_len, tjei_default_ht_chroma_dc),
        ("CHROMA_AC", tjei_default_ht_chroma_ac_len, tjei_default_ht_chroma_ac),
    ]
    
    print("/*")
    print(" * Precomputed Huffman Extended Tables")
    print(" * Generated by generate_huffman_tables.py")
    print(" * ")
    print(" * This file contains precomputed extended Huffman data structures")
    print(" * that eliminate the need for runtime computation.")
    print(" */")
    print()
    print("#ifndef PRECOMPUTED_HUFFMAN_TABLES_H")
    print("#define PRECOMPUTED_HUFFMAN_TABLES_H")
    print()
    print("#include <inttypes.h>")
    print()
    
    # Generate extended tables for each type
    for i, (name, ht_len, ht_val) in enumerate(tables):
        print(f"// ============ {name} ============")
        
        # Calculate spec table length
        spec_len = sum(ht_len)
        print(f"// Spec table length: {spec_len}")
        
        # Generate huffsize and huffcode
        huffsize = get_code_lengths(ht_len)
        huffcode = get_codes(huffsize, spec_len)
        
        # Generate extended tables
        ehuffsize, ehuffcode = get_extended(ht_val, huffsize, huffcode, spec_len)
        
        # Output C arrays
        print(format_c_array(f"tjei_precomputed_ehuffsize_{name.lower()}", ehuffsize, "uint8_t"))
        print(format_c_array(f"tjei_precomputed_ehuffcode_{name.lower()}", ehuffcode, "uint16_t"))
    
    print()
    print("// Array of pointers to precomputed tables for easy indexing")
    print("static const uint8_t* tjei_precomputed_ehuffsize[4] = {")
    print("    tjei_precomputed_ehuffsize_luma_dc,")
    print("    tjei_precomputed_ehuffsize_luma_ac,")
    print("    tjei_precomputed_ehuffsize_chroma_dc,")
    print("    tjei_precomputed_ehuffsize_chroma_ac")
    print("};")
    print()
    print("static const uint16_t* tjei_precomputed_ehuffcode[4] = {")
    print("    tjei_precomputed_ehuffcode_luma_dc,")
    print("    tjei_precomputed_ehuffcode_luma_ac,")
    print("    tjei_precomputed_ehuffcode_chroma_dc,")
    print("    tjei_precomputed_ehuffcode_chroma_ac")
    print("};")
    print()
    print("#endif // PRECOMPUTED_HUFFMAN_TABLES_H")

if __name__ == "__main__":
    main()