#ifndef __base64_h
#define __base64_h

char base64_table[64] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
                         'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
                         'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
                         'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
                         's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2',
                         '3', '4', '5', '6', '7', '8', '9', '+', '/', };


union base64_translator{
    unsigned char input[3];
    struct {
        unsigned char f4:6;
        unsigned char f3:6;
        unsigned char f2:6;
        unsigned char f1:6;
    } __attribute__ ((__packed__));
};

#endif /* __base64_h */
