import jenkins
import zlib
import pdb

def crc32(key):
    return zlib.crc32(bytes(key))

def identity_hash(key):
    return key

def bobhash(key):
    return jenkins.hashlittle(key)

def check_bitmap_digit(bitmap, row, col, tot_rows, tot_cols):
    index = col * tot_rows + row
    return bitmap[index >> 6] & (1 << (index & 63))

def set_bitmap_one(bitmap, row, col, tot_rows, tot_cols):
    # pdb.set_trace()
    index = col * tot_rows + row
    bitmap[index >> 6] |= 1 << (index & 63)
    return bitmap