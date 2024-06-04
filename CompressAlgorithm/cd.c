#include <stdint.h>
#include <stdio.h>
#include "cd.h"
//------------------------------------------------------------------------------
typedef struct
{    // piksele
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t decoded_px; // wskaznik w prostokacie lokalny (lewa gora = 0) zapisu do obrazu zdekodowanego/wyjsciowego w jednostkach pikseli
} cd_int_rect_t;
static cd_int_rect_t g_rect;
static uint8_t g_rect_exists = 0;
//------------------------------------------------------------------------------
// kodowanie
void cd_conv_rect2xywh(cd_rect_10_t* rect, uint16_t* x, uint16_t* y, uint16_t* w, uint16_t* h)
{
    uint64_t z = 0;

    memcpy(&z, rect, sizeof(cd_rect_10_t));

    *x =  z & ((1ULL << 10) - 1);
    *y = (z & ((1ULL << 10) - 1) << 10) >> 10;
    *w = ((z & ((1ULL << 10) - 1) << 20) >> 20)+1;
    *h = ((z & ((1ULL << 10) - 1) << 30) >> 30)+1;
}
//------------------------------------------------------------------------------
void cd_conv_xywh2rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, cd_rect_10_t* rect)
{//do kodowania z 
    uint64_t z = 0;
    z = z | x;
    z = z | (uint64_t) y << 10;
    z = z | (uint64_t) (w-1) << 20;
    z = z | (uint64_t) (h-1) << 30;

    memcpy(rect, &z, sizeof(cd_rect_10_t));
}
//------------------------------------------------------------------------------
int8_t  cd_encode(void* buf_screen,     //wskaznik na caly obraz
    cd_rect_t* rect,                    //prostokat ktory wskazuje jakie dane zakodowac
    void* buf_encoded,                  //bufor na dane zakodowane
    uint32_t* size_buf_encoded          //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie pozakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
    )                                   //zwraca 0-OK lub inna wartosc gdy blad
{
    uint8_t* buf_screen8 = (uint8_t *) buf_screen;
    uint8_t* be = (uint8_t*) buf_encoded;
    uint32_t target_size = *size_buf_encoded;
    uint32_t ptr = 0;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t size;
    uint32_t to_write;
    uint8_t  size_width;    //0: 1 bajt, 1: 2 bajty, 2: 3 bajty, 3: 4bajty
    cd_hdr_raw_t* hdr;

    //dump_frame_hex(buf_screen, 256);
    

    if (ptr + 1 > target_size) return -1;
    hdr = (cd_hdr_raw_t*) &be[ptr];
    ptr++;

    // define type
    hdr->code_type = code_type_raw;

    // define if rectangle exists
    hdr->code_rect_exists = 1;
    if (ptr + sizeof(cd_rect_t) > target_size) return -1;
    
    memcpy(&be[ptr], rect, sizeof(cd_rect_t));
    ptr += sizeof(cd_rect_t);

    cd_conv_rect2xywh(rect, &x, &y, &w, &h);

    size = w * h;

    if (size < 256)
    {
        size_width = 0;
    }
    else if (size < 256 * 256)
        {
            size_width = 1;
        }
        else if (size < 256 * 256 * 256)
            {
                size_width = 2;
            }
            else
            {
                size_width = 3;
            }

    // define size_width
    hdr->size_width = size_width;
    //printf("%d %d\n", (int)size, (int)size_width);
    if (ptr + size_width + 1 > target_size) return -1;
    memcpy(&be[ptr], &size, size_width + 1);
    ptr += size_width + 1;

    // define data
    buf_screen8 += (x * CD_PIXEL_COLOR_DEPTH) / 8 + (y * CD_SCREEN_SIZE_WIDTH * CD_PIXEL_COLOR_DEPTH) / 8;
    to_write = (w * CD_PIXEL_COLOR_DEPTH) / 8;

    for (int i = 0; i < h; i++)
    {
        if (ptr + to_write > target_size) return -1;
        memcpy(&be[ptr], buf_screen8, to_write);
        ptr += to_write;
        buf_screen8 += (CD_SCREEN_SIZE_WIDTH * CD_PIXEL_COLOR_DEPTH)/8;
    }

    *size_buf_encoded = ptr;

    return 0;
}

static uint64_t cd_to_bytes(uint16_t x)
{
    return (((uint64_t)x * CD_PIXEL_COLOR_DEPTH) / 8);
}
//------------------------------------------------------------------------------
int8_t  cd_decode(  void*       buf_encoded,        // wszystkie dane zakodowane
                    uint32_t    size_buf_encoded,   // bajty - rozmiar danych zakodowanych
                    void*       buf_screen,         // wskaznik na caly obraz
                    uint32_t    size_buf_screen     // bajty - rozmiar maksymalny bufora wskazywanego przez buf_screen
)                                                   // zwraca 0: OK lub inna wartosc gdy blad
{
    uint8_t* be = (uint8_t*)buf_encoded;
    uint8_t* bs = (uint8_t*)buf_screen;
    uint32_t control_sum = 0;
    cd_hdr_t *hdr = be++;
    uint8_t type =  hdr->code_type;
    uint8_t rectExists = hdr->code_rect_exists;
    cd_rect_t rect;
    uint32_t size;  // liczba pikseli zakodowana danym algorytmem
    uint32_t size2;

    //dump_frame_hex(buf_encoded, /*size_buf_encoded*/15);
    //printf("%02X \n", type);
    printf("Conversion type: %02X\n", type);

    {
        //odczyt rect
        if (rectExists == 1)
        {
            memcpy(&rect, be, sizeof(rect));
            be += sizeof(rect);

#ifdef CD_RECT_10
            cd_conv_rect2xywh(&rect, &g_rect.x, &g_rect.y, &g_rect.w, &g_rect.h);
#else
            g_rect.x = rect.x;
            g_rect.y = rect.y;
            g_rect.w = rect.w;
            g_rect.h = rect.h;
#endif
            g_rect.decoded_px = 0;

            g_rect_exists = 1;

            printf("%d %d %d %d\n", g_rect.x, g_rect.y, g_rect.w, g_rect.h);
        }
        else
        {
            printf("no %d\n", rectExists);
        }
    }

    {
        //odczyt size
        uint8_t size_width = hdr->size_width + 1;       // number of bytes
        printf("bytes: %d\n", size_width);
        size = 0;
        memcpy(&size, be, size_width);
        size2 = size;
        be += size_width;
    }

switch (type)
{
case code_type_palette:
    break;
case code_type_rre:
case code_type_raw:
    {
        if (!g_rect_exists) return -1;
        // based on where starts a rectangle I setup the bs start position to write
        bs += cd_to_bytes(  (g_rect.y + g_rect.decoded_px / g_rect.w) * CD_SCREEN_SIZE_WIDTH + g_rect.x + g_rect.decoded_px % g_rect.w  );

        uint16_t w = g_rect.w; // piksele

        if ((g_rect.decoded_px) % w != 0)  // czy piksele
        {
            uint16_t d = (w - g_rect.decoded_px % w);  // piksele
            if (size >= d)  // piksele
            {
                uint16_t d_bytes = cd_to_bytes(d);
                if ((control_sum + d_bytes) > size_buf_screen) return -1;
                memcpy(bs, be, d_bytes);
                be += d_bytes;
                bs += d_bytes;       
                g_rect.decoded_px += d;                                                
                size -= d;
                control_sum += d_bytes;
            }
            else
            {
                uint32_t size_bytes = cd_to_bytes(size);
                if ((control_sum + size_bytes) > size_buf_screen) return -1;
                memcpy(bs, be, size_bytes);
                be += size;
                bs += size;
                g_rect.decoded_px += size;
                control_sum += size_bytes;
                size = 0;
            }
        }
        while (size / w > 0)        // piksele
        {
            uint16_t w_bytes = cd_to_bytes(w);
            if ((control_sum + w_bytes) > size_buf_screen) return -1;
            memcpy(bs, be, w_bytes);
            be += w_bytes;
            bs += w_bytes;
            g_rect.decoded_px += w;
            size -= w;
            control_sum += w_bytes;
        }
        if (size > 0)
        {
            uint32_t size_bytes = cd_to_bytes(size);
            if ((control_sum + size_bytes) > size_buf_screen) return -1;
            memcpy(bs, be, size_bytes);
            be += size_bytes;
            bs += size_bytes;
            g_rect.decoded_px += size;
            size = 0;
            control_sum += size_bytes;
        }
        dump_frame_hex(buf_screen, 256);
        printf("control_sum: %u size2: %u\n", control_sum, (size2* CD_PIXEL_COLOR_DEPTH) / 8);
        if (control_sum != (size2 * CD_PIXEL_COLOR_DEPTH) / 8) return -1;
    }
    break;
default:
    printf("Error - compression failed\n");
    return -1;
}
    return 0;
}
//------------------------------------------------------------------------------
