#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

    *x = (uint16_t)(z & ((1ULL << 10) - 1));
    *y = (uint16_t)((z & ((1ULL << 10) - 1) << 10) >> 10);
    *w = (uint16_t)(((z & ((1ULL << 10) - 1) << 20) >> 20)+1);
    *h = (uint16_t)(((z & ((1ULL << 10) - 1) << 30) >> 30)+1);
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
static uint32_t cd_to_bytes(uint16_t x)
{
    return (((uint32_t)x * CD_PIXEL_COLOR_DEPTH) / 8);
}
//------------------------------------------------------------------------------
/*
int8_t  cd_encode(void* buf_screen,     //wskaznik na caly obraz
    cd_rect_t* rect,                    //prostokat ktory wskazuje jakie dane zakodowac - do wywalenia
    void* buf_encoded,                  //bufor na dane zakodowane
    uint32_t* size_buf_encoded          //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie pozakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
    )                                   //zwraca 0-OK lub inna wartosc gdy blad
{
    uint8_t* bs = (uint8_t *) buf_screen;
    uint8_t* be = (uint8_t*) buf_encoded;
    uint32_t ptr = 0;
    uint32_t target_size = *size_buf_encoded;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t size;
    uint32_t to_write;
    uint8_t  size_width;    //0: 1 bajt, 1: 2 bajty, 2: 3 bajty, 3: 4 bajty
    cd_hdr_raw_t* hdr;

    //dump_frame_hex(buf_screen, 256);
    

    if (ptr + 1 > target_size) return -1;
    hdr = (cd_hdr_raw_t*) &be[ptr];
    ptr++;

    // define type
    hdr->code_type = code_type_raw;
    hdr->code_rect_exists = 1;

    if (hdr->code_rect_exists == 1)
    {
        if (ptr + sizeof(cd_rect_t) > target_size) return -1;
        memcpy(&be[ptr], rect, sizeof(cd_rect_t));
        ptr += sizeof(cd_rect_t);
    }

    cd_conv_rect2xywh(rect, &x, &y, &w, &h);

    size = w * h;
    printf("Wymiary: %d %d\n", w, h);

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
    bs += cd_to_bytes(x + (y * CD_SCREEN_SIZE_WIDTH));
    to_write = cd_to_bytes(w);

    for (int i = 0; i < h; i++)
    {
        if (ptr + to_write > target_size) return -1;
        memcpy(&be[ptr], bs, to_write);
        ptr += to_write;
        bs += cd_to_bytes(CD_SCREEN_SIZE_WIDTH);
    }

    *size_buf_encoded = ptr;

    return 0;
}
*/
static void get_pixel(void* image, uint32_t index, cd_color_t* pixel)
{
    uint8_t* image_int = (uint8_t*)image;
    #if (CD_PIXEL_COLOR_DEPTH == 4)
        uint32_t byte_number = index >> 1;
        uint8_t b;
        b = image_int[byte_number];
        if (index & 1)
        {
            pixel->level = b >> 4;
        }
        else
        {
            pixel->level = b & 0xF;
        }
    #endif
 
    #if (CD_PIXEL_COLOR_DEPTH == 8)
        pixel->level = image_int[index];
    #endif
    
    #if (CD_PIXEL_COLOR_DEPTH == 16)
        uint32_t byte_number = index << 1;
        *((uint16_t*)pixel) = *((uint16_t*)&image_int[byte_number]);
        //memcpy(pixel, &image[byte_number], 2);
    #endif

    #if (CD_PIXEL_COLOR_DEPTH == 24)
        uint32_t byte_number = index * sizeof(cd_color_t);
        memcpy(pixel, &image_int[byte_number], sizeof(cd_color_t));
    #endif
}

static void set_pixel(void* image, uint32_t index, cd_color_t* pixel)
{
    uint8_t* image_int = (uint8_t*)image;
    #if (CD_PIXEL_COLOR_DEPTH == 4)
        uint8_t i = index >> 1;
        if (index & 1)
        {
            image_int[i] = (pixel->level << 4) | (image_int[i] & 0x0F);
        }
        else
        {
            image_int[i] = pixel->level | (image_int[i] & 0xF0);
        }
    #endif

    #if (CD_PIXEL_COLOR_DEPTH == 8)
        image_int[index] = pixel->level;
    #endif

    #if (CD_PIXEL_COLOR_DEPTH == 16)
        uint32_t byte_number = index << 1;
        *((uint16_t*)&image_int[byte_number]) = *((uint16_t*)pixel);
    #endif

    #if (CD_PIXEL_COLOR_DEPTH == 24)
        uint32_t byte_number = index * sizeof(cd_color_t);
        memcpy(&image_int[byte_number], pixel, sizeof(cd_color_t));
    #endif
}

static int compare_pixel(void* image1, void* image2, uint32_t index1, uint32_t index2)  //zwraca wartosc niezerowa jezeli sa rozne
{
    cd_color_t p1;
    cd_color_t p2;

    get_pixel(image1, index1, &p1);
    get_pixel(image2, index2, &p2);

    return memcmp(&p1, &p2, sizeof(cd_color_t));
}

typedef struct
{
    uint32_t    index;
    uint32_t    size;
    void        *next;
} strips_list_t;

strips_list_t* list_head;
strips_list_t* list_tail;

static void add_to_list(uint32_t index, uint32_t size)
{
    strips_list_t* new_elem = calloc(sizeof(strips_list_t), 1);
    if (!new_elem)
    {
        printf("%s %d\n", __FUNCTION__, __LINE__);
        return;
    }
    new_elem->index = index;
    new_elem->size = size;
    if (!list_head)
    {
        list_head = new_elem;
        list_tail = new_elem;
    }
    else
    {
        list_tail->next = new_elem;
        list_tail = new_elem;
    }
}

static void split_into_strips(void* prev_buf_screen, void* buf_screen)
{
    uint32_t index = 0;
    int32_t  counter = 0;
    int32_t  index_start = -1;
    uint32_t index_stop;
    uint8_t  started = 0;

    printf("Analiza obrazu\n");
    while (index < CD_SCREEN_SIZE_WIDTH * CD_SCREEN_SIZE_HEIGHT)
    {
        //printf("counter = %d\n", counter);
        if (compare_pixel(buf_screen, prev_buf_screen, index, index) != 0)
        {
            started = 1;
            index_stop = index;
            if (index_start == -1)
            {
                index_start = index;
            }
            counter++;
        }
        else
        {
            if (started)
            {
                counter--;
            }
        }

        if (started)
        {
            if (counter < -100)
            {
                add_to_list(index_start, index_stop - index_start+1);


                counter = 0;
                index_start = -1;
                started = 0;
            }
        }

        index++;
        //if (index >= 100) exit(0);
    }

    if (started)
    {
        add_to_list(index_start, index_stop - index_start + 1);
    }
}

static void print_list()
{
    uint32_t sum = 0;
    uint32_t count = 0;
    strips_list_t* h = list_head;
    while (h)
    {
        printf("I: %6u S: %6u\n", h->index, h->size);
        sum += h->size;
        h = h->next;
        count++;
    }
    printf("Suma: %6u Count: %u\n", sum, count);
}

static void list_free()
{
    strips_list_t* h = list_head;
    strips_list_t* z;
    while (h)
    {
        z = h;
        h = h->next;
        free(z);
    }
    list_head = NULL;
    list_tail = NULL;
}

static uint8_t encode_rre(void* buf_screen, void* buf_encoded, uint32_t* size_buf_encoded)
{

}

static uint8_t encode_raw(void* buf_screen, uint8_t* be, uint32_t* size_buf_encoded, uint8_t is_offset, uint32_t offset, uint32_t pixels_count)
{
    cd_hdr_raw_t* hdr;
    uint32_t ptr = be;                 // TODO CHECK!! tu mi chodzi o wskaznik na miejsce ktore jest nowe w be gdzie mozna zapisywac
    uint32_t target_size = *size_buf_encoded;
    // dodaj header
    

    if (ptr + 1 > target_size) return -1;
    hdr = (cd_hdr_raw_t*)&be[ptr];
    ptr++;

    // define type
    hdr->code_type = code_type_raw;
    hdr->offset_exists = is_offset;

    if (is_offset)
    {
        // dodaj offset
        hdr->offset_exists = offset;
    }

    // dodaj data size
    
}

static uint8_t encode_strips(void* buf_screen, void* buf_encoded, uint32_t* size_buf_encoded)
{
    //int32_t         i;
    //uint32_t        index, size;
    //strips_list_t   *strip = list_head;
    //uint32_t        first_index;
    //cd_color_t      first_pixel;
    //cd_color_t      pixel;
    //uint32_t        offset = 0;
    //uint32_t        the_same_count;
    //uint32_t        raw_count;
    //uint32_t        encoded_count_bytes;
    //uint8_t         *be = (uint8_t*)buf_encoded;

    *size_buf_encoded = 0;
/*
    while (strip)
    {
        //printf("I: %6u S: %6u\n", h->index, h->size);

        index = strip->index;
        size = strip->size;

        if (size < 2)
        {
            //kodowanie raw calego stripu
        }
        else
        {
            get_pixel(buf_screen, index, &first_pixel);

            first_index = index;

            index++;
            
            the_same_count = 1;
            raw_count = 1;

            for (i = 1; i < size; i++, index++)
            {
                get_pixel(buf_screen, index, &pixel);

                if (memcmp(&first_pixel, &pixel, sizeof(cd_color_t)))
                {
                    //rozne
                    raw_count++;
                    if (the_same_count > 1)
                    {
                        //rre
                        //first_index    - wskazuje na pierwszy pixel dla kodowania
                        //the_same_count - przechowuje w tym miejscu ile pikseli musi byc zakodowanych za pomoca rre

                        get_pixel(buf_screen, index, &first_pixel);
                        first_index = index;
                        the_same_count = 1;
                        raw_count = 1;
                        be += encoded_count_bytes;
                        *size_buf_encoded = encoded_count_bytes;
                    }
                    else
                    {
                        //raw
                        //first_index    - wskazuje na pierwszy pixel dla kodowania
                        //A dlugosc jest nieznana
                        //while (1)
                        {
                            if (i+2<size)
                            {
                                //mozna patrzec do przodu

                            }
                            else
                            {
                                //nie mozna patrzec do przodu bo brakuje pixeli

                            }
                        }
                    }
                }
                else
                {   //takie same
                    the_same_count++;
                }
            }
        }
        
        

        //offset = offset + size;

        strip = strip->next;
    }
*/ 
    uint32_t        same = 0;
    uint32_t        not_same = 0;
    cd_color_t      prev;
    cd_color_t      curr;
    cd_color_t      next;
    strips_list_t*  strip = list_head;
    uint32_t        first_index;
    uint32_t        curr_index;
    uint32_t        size1;
    //uint32_t        encoded_count_bytes;
    uint8_t* bs = (uint8_t*)buf_screen;
    uint8_t* be = (uint8_t*)buf_encoded;
    uint8_t         if_offset = 1;
    uint32_t        offset = 0;
        
    // TODO przypadki gdy strip = 1, 2
    while (strip)
    {
        first_index = strip->index;
        curr_index = strip->index+1;
        size1 = strip->size;
        if_offset = 1;
        offset = strip->index - offset;     // na poczatku offset = 0, potem offset zmienia sie na koncu petli, odejmujemy tutaj offset = (index - prev_index)
        for (uint32_t k = 1; k < size1 - 1; k++, curr_index++)
        {
            get_pixel(buf_screen, curr_index -1, &prev);
            get_pixel(buf_screen, curr_index, &curr);
            get_pixel(buf_screen, curr_index +1, &next);
            //porównujemy 3 piksele. Jeœli piksel prev bêdzie 
            /*  Poni¿ej rozpisano 4 ró¿ne stany jakie mog¹ przyj¹æ 3 piksele i co w ka¿dej sytuacji zrobiæ (jakie ma byæ ns, s, czy kodowaæ RAW czy RRE czy jeszcze nie, o ile przesuwamy sprawdzanie).
            *   Funkcja przypisuje do RAW [pocz¹tek ; not_same] w³¹cznie gdzie "not_same" koñczy na pikselu "prev" i "prev" te¿ jest w minipasku do kodowania
            *   Funkcja przypisuje do RRE [pocz¹tek ; same] w³¹cznie gdzie "same" koñczy na pikselu "curr" i "curr" te¿ jest w minipasku do kodowania
                p - prev, 
                c - curr, 
                n - next, 
                ns  - dodaj tyle do not_same, 
                s   - dodaj tyle do same
                RAW - czy koñczymy pasek i kodujemy RAW
                RRE - czy koñczymy pasek i kodujemy RRE

                p | c | n |  ns  |  s  |  RAW  |  RRE  | przesun sprawdzanie o 
                1 = 1 = 1 |  0   |  1  |   0   |   0   | 1
                1 = 1 ! 2 |  0   |  2  |   0   |   1   | 2              - dlaczego? teraz prev i curr s¹ takie same wiêc przesuwamy od razu o 2 ¿eby ich wiêcej nie analizowaæ i od razu zakodowaæ RRE
                1 ! 2 = 2 |  1   |  0  |   1   |   0   | 1
                1 ! 2 ! 3 |  1   |  0  |   0   |   0   | 1

            */
            if(      memcmp(&prev, &curr, sizeof(cd_color_t))  &&  !memcmp(&curr, &next, sizeof(cd_color_t)) )    //if (prev != curr && curr == next)
            {
                not_same++; // tyle bitow zapiszemy do be w kodzie RAW

                // zapisz RAW od poczatku do not_same
                encode_raw(bs, be, size_buf_encoded, if_offset, offset, not_same);

                first_index = curr_index;
                not_same = 0;
                if_offset = 0;
            }
            else if ( !memcmp(&prev, &curr, sizeof(cd_color_t))  &&  memcmp(&curr, &next, sizeof(cd_color_t)) )    //else if (prev == curr && curr != next)
            {
            
                same += 2;
                k++;

                //zapisz RRE od poczatku do same

                first_index = curr_index + 1;
                same = 0;
                if_offset = 0;
            }
            else if ( !memcmp(&prev, &curr, sizeof(cd_color_t))  &&  !memcmp(&curr, &next, sizeof(cd_color_t)) )    { same++;     } //else if (prev == curr && curr == next)
            else                                                                                                    { not_same++; } //else if (prev != curr && curr != next)
        }
        // TODO po tym forze zostana nieprzypisane do kodowania dok³adnie 1-2 piksele, wynika to z tego, ze zapisujemy do prev wlacznie, curr nie zapisujemy chyba ze jest to RRE, ostatniego piksela sprawdzamy tylko jako przyszlego
        // prev - sprawdzony     curr - niesprawdzony     next - nie jest w ogole przypisywany
        offset = strip->index; // zapisujemy poprzedni index w offsecie bo offset nie jest na ten moment potrzebny a potem bedziemy odejmowac
        strip = strip->next;
    }
    return 0;
}

int8_t  cd_encode2(
    void* prev_buf_screen,
    void* buf_screen,                   //wskaznik na caly obraz
    void* buf_encoded,                  //bufor na dane zakodowane
    uint32_t* size_buf_encoded          //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie pozakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
)                                       //zwraca 0-OK lub inna wartosc gdy blad
{
    //uint8_t* strips;        // count; x,y,w,h,x,y,w,h,x,y,w,h,...
    split_into_strips(prev_buf_screen, buf_screen);
    print_list();
    
    encode_strips(buf_screen, buf_encoded, size_buf_encoded);

    list_free();

    return 0;
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
    cd_hdr_t *hdr = (cd_hdr_t*) be++;
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

        uint32_t w = g_rect.w; // piksele
        
        // for the first row, if d >= size or d < size
        if ((g_rect.decoded_px) % w != 0)  // czy piksele
        {
            uint32_t d = (w - g_rect.decoded_px % w);  // pozostalo do w
            uint32_t d_w_size_bytes;
            uint32_t d_w_size;

            if (size >= d)      { d_w_size = d;     }
            else                { d_w_size = size;  }
            d_w_size_bytes = cd_to_bytes(d_w_size);
            if ((control_sum + d_w_size_bytes) > size_buf_screen) return -1;
            memcpy(bs, be, d_w_size_bytes);
            be += d_w_size_bytes;
            bs += d_w_size_bytes + cd_to_bytes(CD_SCREEN_SIZE_WIDTH-w);
            g_rect.decoded_px += d_w_size;
            control_sum += d_w_size_bytes;
            size -= d_w_size;

        }
        // for every next row
        while (size > 0)        // piksele
        {
            uint32_t d_w_size_bytes;
            uint32_t d_w_size;

            if (size / w > 0)   { d_w_size = w;     }
            else                { d_w_size = size;  }

            d_w_size_bytes = cd_to_bytes(d_w_size);
            if ((control_sum + d_w_size_bytes) > size_buf_screen) return -1;
            memcpy(bs, be, d_w_size_bytes);
            be += d_w_size_bytes;
            bs += d_w_size_bytes + cd_to_bytes(CD_SCREEN_SIZE_WIDTH - w);
            g_rect.decoded_px += d_w_size;
            control_sum += d_w_size_bytes;
            size -= d_w_size;
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
