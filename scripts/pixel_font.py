"""Remove rasterizer antialiasing from mkbcfnt A4 texture sheets.

Layout follows libctru font.h TGLP_s. Glyph maps, metrics and sheet layout
are preserved; this is an explicit presentation conversion for Old 3DS.
"""
import struct


def glyph_ink_bounds(data: bytes, codepoint: int):
    """Read native A4 ink bounds; no raster resampling or metric mutation."""
    compact_font(data)  # Validate all file offsets, sections and map links.
    tglp, _, cmap = struct.unpack_from("<III", data, 36)
    glyph = None
    while cmap:
        first, last, method = struct.unpack_from("<HHH", data, cmap)
        if first <= codepoint <= last:
            if method == 0:
                glyph = struct.unpack_from("<H", data, cmap + 12)[0] + codepoint - first
            elif method == 1:
                glyph = struct.unpack_from("<H", data, cmap + 12 + 2 * (codepoint-first))[0]
            else:
                count = struct.unpack_from("<H", data, cmap + 12)[0]
                for i in range(count):
                    cp, index = struct.unpack_from("<HH", data, cmap + 14 + 4*i)
                    if cp == codepoint:
                        glyph = index
                        break
            break
        cmap = struct.unpack_from("<I", data, cmap + 8)[0]
    if glyph is None or glyph == 0xffff:
        raise ValueError("Missing reference glyph")
    cell_w, cell_h = data[tglp: tglp+2]
    sheet_size, sheets, _, columns, rows, width, height, offset = struct.unpack_from("<IHHHHHHI", data, tglp+4)
    sheet, local = divmod(glyph, columns*rows)
    if sheet >= sheets:
        raise ValueError("Invalid reference glyph index")
    gx, gy = (local % columns)*(cell_w+1)+1, (local // columns)*(cell_h+1)+1
    if gx+cell_w > width or gy+cell_h > height:
        raise ValueError("Reference glyph exceeds sheet")
    ink = []
    for y in range(cell_h):
        for x in range(cell_w):
            px, py = gx+x, gy+y
            morton = sum((((px >> i)&1) << (2*i)) | (((py >> i)&1) << (2*i+1)) for i in range(3))
            pixel = ((py//8)*(width//8)+px//8)*64+morton
            alpha = (data[offset+sheet*sheet_size+pixel//2] >> (4*(pixel%2))) & 15
            if alpha:
                ink.append(y)
    if not ink:
        raise ValueError("Empty reference glyph")
    return min(ink), max(ink)+1


def crisp_font(data: bytes) -> bytes:
    if len(data) < 20 or data[:4] not in (b"CFNT", b"CFNU"):
        raise ValueError("Invalid BCFNT header")
    if struct.unpack_from("<H", data, 4)[0] != 0xFEFF:
        raise ValueError("Only little-endian BCFNT is supported")
    position = struct.unpack_from("<H", data, 6)[0]
    if struct.unpack_from("<I", data, 12)[0] != len(data):
        raise ValueError("BCFNT size mismatch")
    result = bytearray(data)
    found = False
    for _ in range(struct.unpack_from("<I", data, 16)[0]):
        if position + 8 > len(data):
            raise ValueError("Truncated BCFNT block")
        size = struct.unpack_from("<I", data, position + 4)[0]
        if size < 8 or position + size > len(data):
            raise ValueError("Invalid BCFNT block size")
        if data[position:position + 4] == b"TGLP":
            if size < 32 or found:
                raise ValueError("Invalid TGLP block")
            sheet_size, sheets, texture_format = struct.unpack_from("<IHH", data, position + 12)
            width, height, offset = struct.unpack_from("<HHI", data, position + 24)
            if texture_format != 11:  # libctru GPU_A4
                raise ValueError("Expected A4 font sheets")
            end = offset + sheet_size * sheets
            if not sheets or sheet_size != width * height // 2 or offset < position + 32 or end > len(data):
                raise ValueError("Invalid A4 sheet bounds")
            for i in range(offset, end):
                value = data[i]
                result[i] = (0xF0 if value >> 4 >= 8 else 0) | (0x0F if value & 15 >= 8 else 0)
            found = True
            # mkbcfnt declares a 32-byte TGLP header; sheet bytes follow separately.
            position = max(position + size, end)
        else:
            position += size
    if not found:
        raise ValueError("Missing TGLP")
    return bytes(result)


def compact_font(data: bytes) -> bytes:
    """Crop transparent bottom tile rows of a single A4 sheet, without repacking
    glyphs. Keep horizontal stride; relocate every supported CFNT file pointer.
    Multi-sheet fonts retain their layout, as cropping would change sheet IDs.
    """
    crisp_font(data)  # Validate texture bounds before following file pointers.
    if data[:4] != b"CFNT" or struct.unpack_from("<I",data,8)[0]!=0x03000000:
        raise ValueError("Compaction requires on-disk CFNT offsets")
    position=struct.unpack_from("<H",data,6)[0]
    blocks=[]
    for _ in range(struct.unpack_from("<I",data,16)[0]):
        if position+8>len(data): raise ValueError("Truncated font section")
        signature,size=struct.unpack_from("<4sI",data,position)
        minimum={b"FINF":32,b"TGLP":32,b"CWDH":16,b"CMAP":24}.get(signature)
        if minimum is None or size<minimum or position+size>len(data):
            raise ValueError("Unsupported or invalid font section")
        blocks.append((signature,position,size))
        if signature==b"TGLP":
            sheet_size,sheets=struct.unpack_from("<IH",data,position+12)
            sheet_offset=struct.unpack_from("<I",data,position+28)[0]
            position=max(position+size,sheet_offset+sheet_size*sheets)
        else: position+=size
    if position!=len(data): raise ValueError("Unparsed font data")
    finfs=[p for sig,p,_ in blocks if sig==b"FINF"]
    textures=[p for sig,p,_ in blocks if sig==b"TGLP"]
    if len(finfs)!=1 or len(textures)!=1: raise ValueError("Ambiguous font sections")
    finf,tglp=finfs[0],textures[0]
    size,sheets,fmt,columns,rows,width,height,offset=struct.unpack_from("<IHHHHHHI",data,tglp+12)
    cell_width,cell_height=data[tglp+8],data[tglp+9]
    if not columns or not rows or width<8 or height<8 or width&(width-1) or height&(height-1):
        raise ValueError("Invalid sheet dimensions")
    if columns!=width//(cell_width+1) or rows!=height//(cell_height+1):
        raise ValueError("Invalid glyph grid")
    body_types={p+8:sig for sig,p,_ in blocks}
    pointers=[(finf+16,b"TGLP"),(finf+20,b"CWDH"),(finf+24,b"CMAP")]
    maximum=struct.unpack_from("<H",data,finf+10)[0]  # replacement glyph
    for sig,p,length in blocks:
        if sig==b"CWDH":
            start,end=struct.unpack_from("<HH",data,p+8)
            if start>end: raise ValueError("Invalid glyph range")
            maximum=max(maximum,end)  # conservative for mkbcfnt exclusive end
            pointers.append((p+12,b"CWDH"))
        elif sig==b"CMAP":
            first,last,method=struct.unpack_from("<HHH",data,p+8)
            if first>last: raise ValueError("Invalid character range")
            pointers.append((p+16,b"CMAP"))
            if method==0:
                maximum=max(maximum,struct.unpack_from("<H",data,p+20)[0]+last-first)
            elif method==1:
                count=last-first+1
                if 20+2*count>length: raise ValueError("Truncated character table")
                indices=struct.unpack_from("<"+"H"*count,data,p+20)
                maximum=max([maximum]+[i for i in indices if i!=0xffff])
            elif method==2:
                count=struct.unpack_from("<H",data,p+20)[0]
                if 22+4*count>length: raise ValueError("Truncated character scan")
                for i in range(count): maximum=max(maximum,struct.unpack_from("<H",data,p+24+4*i)[0])
            else: raise ValueError("Unsupported character map")
    for field,kind in pointers:
        pointer=struct.unpack_from("<I",data,field)[0]
        if pointer and body_types.get(pointer)!=kind: raise ValueError("Invalid font pointer")
    for first_field,next_offset in ((finf+20,4),(finf+24,8)):
        pointer=struct.unpack_from("<I",data,first_field)[0]
        visited=set()
        while pointer:
            if pointer in visited: raise ValueError("Cyclic font pointer chain")
            visited.add(pointer)
            pointer=struct.unpack_from("<I",data,pointer+next_offset)[0]
    if struct.unpack_from("<I",data,finf+16)[0]!=tglp+8:
        raise ValueError("Missing glyph texture reference")
    if maximum>=columns*rows*sheets: raise ValueError("Glyph index outside sheet")
    if sheets!=1: return data
    required=((maximum+1+columns-1)//columns)*(cell_height+1)
    new_height=8
    while new_height<required: new_height*=2
    if new_height>=height: return data
    new_size=width*new_height//2
    cut_start,cut_end=offset+new_size,offset+size
    if any(data[cut_start:cut_end]): raise ValueError("Cannot discard nontransparent pixels")
    delta=cut_end-cut_start
    result=bytearray(data[:cut_start]+data[cut_end:])
    def relocated(value):
        if cut_start<=value<cut_end: raise ValueError("Pointer into removed texture")
        return value-delta if value>=cut_end else value
    struct.pack_into("<I",result,12,len(result))
    struct.pack_into("<I",result,tglp+12,new_size)
    struct.pack_into("<H",result,tglp+22,new_height//(cell_height+1))
    struct.pack_into("<H",result,tglp+26,new_height)
    for field,_ in pointers:
        pointer=struct.unpack_from("<I",data,field)[0]
        struct.pack_into("<I",result,relocated(field),relocated(pointer) if pointer else 0)
    crisp_font(bytes(result))
    return bytes(result)
