u8 vt50rom[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 32 ' '
    0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, // 33 '!'
    0x00, 0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, // 34 '"'
    0x00, 0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A, // 35 '#'
    0x00, 0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04, // 36 '$'
    0x00, 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03, // 37 '%'
    0x00, 0x08, 0x14, 0x14, 0x08, 0x15, 0x12, 0x0D, // 38 '&'
    0x00, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, // 39 '''
    0x00, 0x04, 0x08, 0x10, 0x10, 0x10, 0x08, 0x04, // 40 '('
    0x00, 0x04, 0x02, 0x01, 0x01, 0x01, 0x02, 0x04, // 41 ')'
    0x00, 0x04, 0x15, 0x0E, 0x04, 0x0E, 0x15, 0x04, // 42 '*'
    0x00, 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00, // 43 '+'
    0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08, // 44 ','
    0x00, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, // 45 '-'
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, // 46 '.'
    0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00, // 47 '/'
    0x00, 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E, // 48 '0'
    0x00, 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E, // 49 '1'
    0x00, 0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F, // 50 '2'
    0x00, 0x1F, 0x01, 0x02, 0x06, 0x01, 0x11, 0x0E, // 51 '3'
    0x00, 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02, // 52 '4'
    0x00, 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E, // 53 '5'
    0x00, 0x07, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E, // 54 '6'
    0x00, 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08, // 55 '7'
    0x00, 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E, // 56 '8'
    0x00, 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x1C, // 57 '9'
    0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, // 58 ':'
    0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x08, // 59 ';'
    0x00, 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02, // 60 '<'
    0x00, 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00, // 61 '='
    0x00, 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, // 62 '>'
    0x00, 0x0E, 0x11, 0x02, 0x04, 0x04, 0x00, 0x04, // 63 '?'
    0x00, 0x0E, 0x11, 0x15, 0x17, 0x16, 0x10, 0x0F, // 64 '@'
    0x00, 0x04, 0x0A, 0x11, 0x11, 0x1F, 0x11, 0x11, // 65 'A'
    0x00, 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E, // 66 'B'
    0x00, 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E, // 67 'C'
    0x00, 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E, // 68 'D'
    0x00, 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F, // 69 'E'
    0x00, 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10, // 70 'F'
    0x00, 0x0F, 0x10, 0x10, 0x10, 0x13, 0x11, 0x0F, // 71 'G'
    0x00, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, // 72 'H'
    0x00, 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E, // 73 'I'
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x11, 0x0E, // 74 'J'
    0x00, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, // 75 'K'
    0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, // 76 'L'
    0x00, 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11, // 77 'M'
    0x00, 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, // 78 'N'
    0x00, 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, // 79 'O'
    0x00, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10, // 80 'P'
    0x00, 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D, // 81 'Q'
    0x00, 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11, // 82 'R'
    0x00, 0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E, // 83 'S'
    0x00, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, // 84 'T'
    0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, // 85 'U'
    0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04, // 86 'V'
    0x00, 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11, // 87 'W'
    0x00, 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11, // 88 'X'
    0x00, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, // 89 'Y'
    0x00, 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F, // 90 'Z'
    0x00, 0x1F, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F, // 91 '['
    0x00, 0x00, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, // 92 '\'
    0x00, 0x1F, 0x03, 0x03, 0x03, 0x03, 0x03, 0x1F, // 93 ']'
    0x00, 0x00, 0x00, 0x04, 0x0A, 0x11, 0x00, 0x00, // 94 '^'
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, // 95 '_'
};
