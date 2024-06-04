#pragma once
//------------------------------------------------------------------------------
//#define PACKED __attribute__((packed))
//------------------------------------------------------------------------------
#define CD_SCREEN_SIZE_WIDTH        1024        // w - liczba pikseli
#define CD_SCREEN_SIZE_HEIGHT       600         // h - liczba pikseli
#define CD_PIXEL_COLOR_DEPTH        16          // liczba bitow na piksel
#pragma pack(push,1)
//------------------------------------------------------------------------------
enum
{
        code_type_palette,                      //Header, Rect, Size, Palette, Dane
        code_type_rre,                          //Header, Rect, Size, Color
        code_type_raw,                          //Header, Rect, Size, Color, Color ....
};
//------------------------------------------------------------------------------
typedef struct
{
    uint8_t     code_type               :   2;
    uint8_t     code_rect_exists        :   1;
    uint8_t     size_width              :   2;
    uint8_t     reserve                 :   3;
} cd_hdr_t;             //1 bajt
//------------------------------------------------------------------------------
typedef struct
{
    uint8_t     code_type               :   2;
    uint8_t     code_rect_exists        :   1;
    uint8_t     size_width              :   2;
    uint8_t     code_palette_exists     :   1;
    uint8_t     color_palette_count     :   2;  //liczba kolorow kodujacych w palecie minimum 2 kolory. Wartosc pola moze sie zmieniac od 0 do 15, z tym ze 0 oznacza 2 kolory, 1 - oznaczy 3 kolory itd. 2- oznacza 4 kolory. 15 oznacza 17 kolorow
} cd_hdr_palette_t;     //1 bajt
//------------------------------------------------------------------------------
typedef struct
{
    uint8_t     code_type               :   2;
    uint8_t     code_rect_exists        :   1;
    uint8_t     size_width              :   2;  //mowi o ilosci bajtow pola Size. Wartosc pola 0 oznacza ze dlugosc pola Size wynosi 1 bajt, 1 - 2 bajtym 2 - 3 bajty 3 - bajty
    uint8_t     reserve                 :   3;
} cd_hdr_rre_t;         //1 bajt
//------------------------------------------------------------------------------ 
typedef struct
{
    uint8_t     code_type               :   2;
    uint8_t     code_rect_exists        :   1;
    uint8_t     size_width              :   2;  //mowi o ilosci bajtow pola Size. Wartosc pola 0 oznacza ze dlugosc pola Size wynosi 1 bajt, 1 - 2 bajtym 2 - 3 bajty 3 - bajty
    uint8_t     reserve                 :   3;
} cd_hdr_raw_t;         //1 bajt
//------------------------------------------------------------------------------
typedef struct
{
    uint8_t     x;
    uint8_t     y;
    uint8_t     w;      //indeksowanie od 0
    uint8_t     h;      //indeksowanie od 0
} cd_rect_8_t;          //4 bajty
//------------------------------------------------------------------------------
typedef struct
{
    uint8_t     t[5];
 } cd_rect_10_t;        //5 bajtow
//------------------------------------------------------------------------------
typedef struct
{
    uint8_t     level_0                 :   4;
    uint8_t     level_1                 :   4;
} cd_color_2x4_t;
//------------------------------------------------------------------------------
typedef struct
{
    uint8_t     level;
} cd_color_8_t;
//------------------------------------------------------------------------------
typedef struct
{
    uint16_t     r                       :   5;
    uint16_t     g                       :   6;
    uint16_t     b                       :   5;
} cd_color_16_t;
//------------------------------------------------------------------------------
typedef struct
{
    uint8_t     r;
    uint8_t     g;
    uint8_t     b;
} cd_color_24_t;
#pragma pack()
//------------------------------------------------------------------------------
#if (CD_PIXEL_COLOR_DEPTH == 4)
    //typedef cd_color_2x4_t cd_color_t;
    #define cd_color_t      cd_color_8_t
#endif
//------------------------------------------------------------------------------
#if (CD_PIXEL_COLOR_DEPTH == 8)
    #define cd_color_t      cd_color_8_t
#endif
//------------------------------------------------------------------------------
#if (CD_PIXEL_COLOR_DEPTH == 16)
    #define cd_color_t      cd_color_16_t
#endif
//------------------------------------------------------------------------------
#if (CD_PIXEL_COLOR_DEPTH == 24)
    #define cd_color_t      cd_color_24_t
#endif
//------------------------------------------------------------------------------
#if (CD_SCREEN_SIZE_WIDTH > 256) || (CD_SCREEN_SIZE_HEIGHT > 256)
    #define CD_RECT_10
#else
    #define CD_RECT_8
#endif
//------------------------------------------------------------------------------
#ifdef CD_RECT_10
    #define cd_rect_t   cd_rect_10_t
#else
    #define cd_rect_t   cd_rect_8_t
#endif
//------------------------------------------------------------------------------
int8_t  cd_encode(  void        *buf_screen,            //wskaznik na caly obraz
                    cd_rect_t   *rect,                  //prostokat ktory wskazuje jakie dane zakodowac
                    void        *buf_encoded,           //bufora na dane zakodowane
                    uint32_t    *size_buf_encoded       //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie po zakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
                 );                                     //zwraca 0-OK lub inna wartosc gdy blad
//------------------------------------------------------------------------------
int8_t  cd_decode(  void        *buf_encoded,           //bufora na dane zakodowane
                    uint32_t    size_buf_encoded,       //rozmiar danych zakodowanych
                    void        *buf_screen,            //wskaznik na caly obraz
                    uint32_t    size_buf_screen         //rozmiar maksymalny bufora wskazywanego przez buf_screen
                 );                                     //zwraca 0-OK lub inna wartosc gdy blad
//------------------------------------------------------------------------------
void cd_conv_rect2xywh(cd_rect_10_t* rect, uint16_t* x, uint16_t* y, uint16_t* w, uint16_t* h);
void cd_conv_xywh2rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, cd_rect_10_t* rect);
//------------------------------------------------------------------------------


void dump_frame_hex(void* buf, int size);

