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
    uint32_t    index;  // pixel index
    uint32_t    size;   // pixels
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
            if (counter < 0)
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
uint32_t liczba_bajto_do_zakodowania;
static void print_list()
{
    liczba_bajto_do_zakodowania = 0;
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
    liczba_bajto_do_zakodowania = (sum * CD_PIXEL_COLOR_DEPTH) / 8;
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

static uint8_t encode_rre(
    void* buf_screen,
    void* buf_encoded,
    uint32_t max_size_be_bytes,
    uint32_t* curr_size_bytes,        // rozmiar w bajtach zakodowanych danych
    uint8_t if_index,
    uint32_t index_p,
    uint32_t pixels_count
)
{
    cd_hdr_rre_t* hdr;
    uint8_t* be = (uint8_t*)buf_encoded;

    uint32_t ptr = *curr_size_bytes;
    uint8_t offset_size;
    uint8_t data_size;

    // | H | [O] | S | D: Color |
    // H - header, little endian example: 01 10 10
    if (ptr + sizeof(cd_hdr_rre_t) > max_size_be_bytes) return -1;
    hdr = (cd_hdr_rre_t*)&be[ptr];
    hdr->code_type = code_type_rre;

    if (if_index)
    {
        offset_size = convert_to_data_size(index_p) + 1;    // 0: brak offsetu, 1: 1 bajt, 2: 2 bajty, 3: 3 bajty 
    }
    else offset_size = 0;

    hdr->offset_size = offset_size;

    data_size = convert_to_data_size(pixels_count);
    hdr->data_size = data_size;        // 0: 1 bajt, 1: 2 bajty, 2: 3 bajty, 3: 4 bajty
    data_size++;        // w dalszej czesci kodu bedzie uzywana wielkosc w bajtach
    ptr++;

    // O - index/offset if exists
    if (if_index)
    {
        if (ptr + offset_size > max_size_be_bytes) return -1;
        memcpy(&be[ptr], &index_p, offset_size);
        ptr += offset_size;
    }

    // S - size data
    if (ptr + data_size > max_size_be_bytes) return -1;
    memcpy(&be[ptr], &pixels_count, data_size);
    ptr += data_size;

    // D - data = 1 color
    uint32_t bites_to_encode_data = CD_PIXEL_COLOR_DEPTH;
    uint32_t bytes_to_encode_data = bites_to_encode_data/8 + (bites_to_encode_data % 8 ? 1:0);
    if (ptr + bytes_to_encode_data > max_size_be_bytes) return -1;
    cd_color_t pixel;
    get_pixel(buf_screen, index_p, &pixel);
    set_pixel(&be[ptr], 0, &pixel);
    ptr += bytes_to_encode_data;

    *curr_size_bytes = ptr;
}

static uint32_t convert_to_data_size(uint32_t x)
{
    if (x < 256)                   x = 0;  // 1 bajt
    else if (x < 256 * 256)        x = 1;  // 2 bajty
    else if (x < 256 * 256 * 256)  x = 2;  // 3 bajty
    else                           x = 3;  // 4 bajty
    return x;
}

static uint8_t encode_raw(
    void* buf_screen, 
    void* buf_encoded, 
    uint32_t max_size_be_bytes, 
    uint32_t *curr_size_bytes,        // rozmiar w bajtach zakodowanych danych
    uint8_t if_index, 
    uint32_t index_p, 
    uint32_t pixels_count
)
{
    cd_hdr_raw_t* hdr;
    uint8_t* be = (uint8_t*)buf_encoded;

    uint32_t ptr = *curr_size_bytes;
    uint8_t offset_size;
    uint8_t data_size;
    
    // | H | [O] | S | D |
    // H - header, little endian example: 01 10 1 10
    if (ptr + sizeof(cd_hdr_raw_t) > max_size_be_bytes) return -1;
    hdr = (cd_hdr_raw_t*)&be[ptr];
    hdr->code_type = code_type_raw;

    if (if_index)
    {
        offset_size = convert_to_data_size(index_p)+1;    // 0: brak offsetu, 1: 1 bajt, 2: 2 bajty, 3: 3 bajty 
    }
    else offset_size = 0;

    hdr->offset_size = offset_size;

    data_size = convert_to_data_size(pixels_count);
    hdr->data_size = data_size;        // 0: 1 bajt, 1: 2 bajty, 2: 3 bajty, 3: 4 bajty
    data_size++;        // w dalszej czesci kodu bedzie uzywana wielkosc w bajtach
    ptr++;

    // O - index/offset if exists
    if (if_index)
    {
        if (ptr + offset_size > max_size_be_bytes) return -1;
        memcpy(&be[ptr], &index_p, offset_size);
        ptr += offset_size;
    }

    // S - size data
    if (ptr + data_size > max_size_be_bytes) return -1;
    memcpy(&be[ptr], &pixels_count, data_size);
    ptr += data_size;


    // D - data
    uint32_t bites_to_encode_data = CD_PIXEL_COLOR_DEPTH * pixels_count;
    uint32_t bytes_to_encode_data = bites_to_encode_data/8 + (bites_to_encode_data % 8 ? 1:0);
    if (ptr + bytes_to_encode_data > max_size_be_bytes) return -1;

    cd_color_t pixel;
    uint8_t* data_ptr = &be[ptr];
    for (uint32_t i = 0; i < pixels_count; i++)
    {
        get_pixel(buf_screen, index_p++, &pixel);
        set_pixel(data_ptr, i, &pixel);
    }
    ptr += bytes_to_encode_data;

    *curr_size_bytes = ptr;
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

    // *size_buf_encoded = 0;
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

     //uint32_t        encoded_count_bytes;
    uint32_t ptr = 0;

    // buf_encoded
    uint32_t curr_size_bytes = 0; // ile jest 

    // Obs³uga 1 paska:
    strips_list_t*  strip = list_head;
    uint32_t        first_i;
    uint32_t        first_index;
    uint32_t        curr_index;
    uint32_t        strip_size;
    uint32_t        same = 0;
    uint32_t        not_same = 0;

    // Obs³uga pikseli
    cd_color_t      prev;
    cd_color_t      curr;
    cd_color_t      next;
    
    // Obs³uga offsetu w buff_encoded
    uint8_t         if_index = 1;
    uint32_t        offset = 0;
    int licznikRAW = 0, licznikRRE = 0;

    while (strip)
    {
        first_i = strip->index;
        first_index = strip->index;
        curr_index = strip->index + 1;
        strip_size = strip->size; // pixels
        if_index = 1;
        offset = strip->index - offset;     // na poczatku offset = 0, potem offset zmienia sie na koncu petli, odejmujemy tutaj offset = (index - prev_index)
        same = 0;
        not_same = 0;
        // printf("Strip:  first_i: %d, curr_index: %d strip_size: %d\n", first_i, curr_index, strip_size);
        // Ten for sprawdza ca³y pasek minus 1-2 ostatnie piksele, to ile ich zostanie zale¿y od ostatniej kompresji RRE

        for (uint32_t k = 1; k < strip_size - 1; k++, curr_index++)
        {
            get_pixel(buf_screen, curr_index - 1, &prev);
            get_pixel(buf_screen, curr_index, &curr);
            get_pixel(buf_screen, curr_index + 1, &next);
            // Porównujemy 3 piksele.
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
            if (memcmp(&prev, &curr, sizeof(cd_color_t)) && !memcmp(&curr, &next, sizeof(cd_color_t)))    //if (prev != curr && curr == next)
            {
                not_same++; // tyle pikseli zapiszemy do be w kodzie RAW

                // zapisz RAW od poczatku do not_same
                encode_raw(buf_screen, buf_encoded, *size_buf_encoded, &curr_size_bytes, if_index, first_index, not_same);
                first_index = curr_index;
                not_same = 0;
                if_index = 0;
                licznikRAW++;
            }
            else if (!memcmp(&prev, &curr, sizeof(cd_color_t)) && memcmp(&curr, &next, sizeof(cd_color_t)))    //else if (prev == curr && curr != next)
            {
                same += 2;
                //zapisz RRE od poczatku do same 
                encode_rre(buf_screen, buf_encoded, *size_buf_encoded, &curr_size_bytes, if_index, first_index, same);

                first_index = curr_index + 1;
                same = 0;
                if_index = 0;
                k++;
                curr_index++;
                licznikRRE++;
            }
            else if (!memcmp(&prev, &curr, sizeof(cd_color_t)) && !memcmp(&curr, &next, sizeof(cd_color_t))) { same++; } //else if (prev == curr && curr == next)
            else { not_same++; } //else if (prev != curr && curr != next)
        }

        // Obs³uga 2 ostatnich pikseli w ca³ym obrazie + ci¹g, który nie zosta³ zapisany w poprzednich iteracjach
        uint32_t last_pixels = strip_size - (curr_index - first_i+1); // define number of pixels to write
        get_pixel(buf_screen, curr_index - 1, &prev);
        if (last_pixels == 2) get_pixel(buf_screen, curr_index, &curr);
        if (last_pixels == 1)
        {
            // printf("-----------------------------------------------------------------------------\n");
            not_same++;
            encode_raw(buf_screen, buf_encoded, size_buf_encoded, &curr_size_bytes, if_index, curr_index-1, not_same); // prev do RAW
        }
        else if(same > 0)
        {
            if (!memcmp(&prev, &curr, sizeof(cd_color_t))) //if prev == curr
            {
                same += 2;
                encode_rre(buf_screen, buf_encoded, size_buf_encoded, &curr_size_bytes, if_index, first_index, same);
            }
            else //if prev != curr
            {
                encode_rre(buf_screen, buf_encoded, size_buf_encoded, &curr_size_bytes, if_index, first_index, same);
                same = 0;
                not_same = 2;
                encode_raw(buf_screen, buf_encoded, size_buf_encoded, &curr_size_bytes, if_index, first_index, not_same);
            }
 
        }
        else  // ca³a reszta z 2 ostatnimi niezale¿nie czy sa takie same do RAW, bo i tak zajm¹ tyle samo bajtów
        {
            not_same += 2;
            encode_raw(buf_screen, buf_encoded, size_buf_encoded, &curr_size_bytes, if_index, first_index, not_same);
        }

        // printf("not_same: %d same: %d\n", not_same, same);

        // prev - sprawdzony     curr - niesprawdzony     next - nie jest w ogole przypisywany
        offset = strip->index; // zapisujemy poprzedni index w offsecie bo offset nie jest na ten moment potrzebny a potem bedziemy odejmowac
        strip = strip->next;
    }
    
    printf("Licznik RAW: %d  RRE: %d\n", licznikRAW, licznikRRE);
    //printf("Buf encoded size: %d   Original size: %d   Ratio: %3.2f%%\n", *size_buf_encoded, cd_to_bytes(CD_SCREEN_SIZE_WIDTH* CD_SCREEN_SIZE_HEIGHT), 100.0-(double)((double)(*size_buf_encoded) / (double)(cd_to_bytes(CD_SCREEN_SIZE_WIDTH * CD_SCREEN_SIZE_HEIGHT))) * (double)100.0);
    *size_buf_encoded = curr_size_bytes;
    uint32_t l_bajtow_na_obraz_oryginal = CD_SCREEN_SIZE_WIDTH* CD_SCREEN_SIZE_HEIGHT* CD_PIXEL_COLOR_DEPTH / 8;

    printf("Buf encoded size: %u   Original size: %d   Ratio: %3.2f%% Encoded data size: %u\n", curr_size_bytes, l_bajtow_na_obraz_oryginal, 100 - (float)curr_size_bytes / l_bajtow_na_obraz_oryginal * 100.0F, liczba_bajto_do_zakodowania);
    return 0;
}
//------------------------------------------------------------------------------//------------------------------------------------------------------------------
int8_t  cd_encode2(
    void* prev_buf_screen,
    void* buf_screen,                   //wskaznik na caly obraz
    void* buf_encoded,                  //bufor na dane zakodowane
    uint32_t* size_buf_encoded          //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie po zakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
)                                       //zwraca 0: OK lub inna wartosc gdy blad
{
    //uint8_t* strips;        // count; x,y,w,h,x,y,w,h,x,y,w,h,...
    split_into_strips(prev_buf_screen, buf_screen);
    print_list();
    
    encode_strips(buf_screen, buf_encoded, size_buf_encoded);

    list_free();

    return 0;
}

//------------------------------------------------------------------------------//------------------------------------------------------------------------------

int8_t  cd_decode(  void*       buf_encoded,        // wszystkie dane zakodowane
                    uint32_t    size_buf_encoded,   // bajty - rozmiar danych zakodowanych
                    void*       buf_screen,         // wskaznik na caly obraz
                    uint32_t    size_buf_screen     // bajty - rozmiar maksymalny bufora wskazywanego przez buf_screen
)                                                   // zwraca 0: OK lub inna wartosc gdy blad
{
    uint8_t* be = (uint8_t*)buf_encoded;
    cd_hdr_t* hdr;
    cd_color_t pixel;
    uint32_t ptr = 0;
    uint32_t offset = 0;
    while (1)
    {
        if (ptr + 1 > size_buf_encoded) return -1;
        hdr = be++;
        ptr++;

        switch (hdr->code_type)
        {
        case code_type_raw:
        {
            cd_hdr_raw_t* hdr_raw = (cd_hdr_raw_t*)hdr;
            uint8_t offset_size = hdr_raw->offset_size;
            uint8_t data_size = hdr_raw->data_size+1;
            uint32_t pixels_count = 0;
            if (offset_size)
            {
                offset = 0;
                if (ptr + offset_size > size_buf_encoded) return -1;
                memcpy(&offset, be, offset_size);
                be += offset_size;
                ptr += offset_size;
            }
            if (ptr + data_size > size_buf_encoded) return -1;
            memcpy(&pixels_count, be, data_size);
            be += data_size;
            ptr += data_size;
            uint32_t bites_to_encode_data = CD_PIXEL_COLOR_DEPTH * pixels_count;
            uint32_t bytes_to_encode_data = bites_to_encode_data / 8 + (bites_to_encode_data % 8 ? 1 : 0);
            if (ptr + bytes_to_encode_data > size_buf_encoded) return -1;
            for (uint32_t i = 0; i < pixels_count; i++)
            {
                get_pixel(be, i, &pixel);
                set_pixel(buf_screen, offset++, &pixel);
            }
            be += bytes_to_encode_data;
            ptr += bytes_to_encode_data;
            uint32_t decoded_bytes = sizeof(cd_hdr_raw_t) + offset_size + data_size + bytes_to_encode_data;
            if (ptr == size_buf_encoded)
                {
                printf("Koniec dekodowania: %u\n", ptr);
                return 0;
            }
            if (ptr > size_buf_encoded)
            {
                printf("Bledne dane.\n");
                return -1;
            }
            break;
        }
        case code_type_rre:
        {
            cd_hdr_rre_t* hdr_rre = (cd_hdr_rre_t*)hdr;
            uint8_t offset_size = hdr_rre->offset_size;
            uint8_t data_size = hdr_rre->data_size + 1;
            uint32_t pixels_count = 0;
            if (offset_size)
            {
                offset = 0;
                if (ptr + offset_size > size_buf_encoded) return -1;
                memcpy(&offset, be, offset_size);
                be += offset_size;
                ptr += offset_size;
            }
            if (ptr + data_size > size_buf_encoded) return -1;
            memcpy(&pixels_count, be, data_size);
            be += data_size;
            ptr += data_size;
            uint32_t bites_to_encode_data = CD_PIXEL_COLOR_DEPTH;
            uint32_t bytes_to_encode_data = bites_to_encode_data / 8 + (bites_to_encode_data % 8 ? 1 : 0);
            if (ptr + bytes_to_encode_data > size_buf_encoded) return -1;
            //uint32_t bites_to_decoded_data = CD_PIXEL_COLOR_DEPTH * pixels_count;
            //uint32_t bytes_to_decoded_data = bites_to_decoded_data / 8 + (bites_to_decoded_data % 8 ? 1 : 0);
            //if (ptr + bytes_to_decoded_data > size_buf_screen) return -1;
            get_pixel(be, 0, &pixel);

            for (uint32_t i = 0; i < pixels_count; i++)
            {
                set_pixel(buf_screen, offset++, &pixel);
            }
            be += bytes_to_encode_data;
            ptr += bytes_to_encode_data;
            uint32_t decoded_bytes = sizeof(cd_hdr_rre_t) + offset_size + data_size + bytes_to_encode_data;
            if (ptr == size_buf_encoded)
            {
                printf("Koniec dekodowania: %u\n", ptr);
                return 0;
            }
            if (ptr > size_buf_encoded)
            {
                printf("Bledne dane.\n");
                return -1;
            }
            break;
        }
        default: 
            printf("Nieobslugiwany standard dekodowania. \n");
            return -1;

        }
    }
    return 0;
}
//------------------------------------------------------------------------------
