// CompressAlgorithm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <conio.h> 
#include <string.h> 
#include <io.h>      // Nag³ówek dla close() i _read()

#include "cd.h"


void dump_frame_hex(void* buf, int size)
{
    uint8_t* b = buf;
    for (int i = 0; i < size; i++)
    {
        if (i % 16 == 0)
        {
            printf("\n");
        
        }
        printf("%02X ", *b++);
        
    }
    printf("\n");
}

long getFileSize(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Unable to open file");
        return -1;
    }

    // Przejœcie na koniec pliku
    if (fseek(file, 0, SEEK_END) != 0) {
        printf("fseek failed");
        fclose(file);
        return -1;
    }

    // Pobranie pozycji wskaŸnika, która odpowiada rozmiarowi pliku
    long fileSize = ftell(file);
    if (fileSize == -1) {
        printf("ftell failed");
    }

    // Zamkniêcie pliku
    fclose(file);

    return fileSize;
}

static void *alloc_read_file(char * filename, uint32_t *file_size)
{
    int ret;
    long size = getFileSize(filename);

    printf("Otwieranie pliku %s pliku o rozmiarze %d\n", filename, size);
    
    void* buf = malloc(size);

    if (!buf)
    {
        return NULL;
    }

    int fd = _open(filename, O_RDONLY);
    if (fd == -1)
    {
        printf("Error opening file %s\n", filename);
        return NULL;
    }
    ret = _read(fd, buf, size);
    _close(fd);

    *file_size = size;
    return buf;
}

static void save_to_file(char* filename, void * buf, uint32_t size)
{
    int fd = _open(filename, O_CREAT | O_BINARY | O_WRONLY | O_TRUNC);
    if (fd == -1) {
        printf("Error opening file %s\n", filename);
        //return EXIT_FAILURE;
        exit(-1);
    }
    else
    {
        int r = _write(fd, buf, size);
        _close(fd);
    }
}

int main()
{
    int8_t ret;
    uint32_t  size_buf_encoded = ((CD_SCREEN_SIZE_WIDTH * CD_SCREEN_SIZE_HEIGHT * CD_PIXEL_COLOR_DEPTH) / 8) * 2;
    void* buf_encoded = calloc(size_buf_encoded, 1);
    //cd_rect_t rect;
    
    uint32_t size_buf_screen = (CD_SCREEN_SIZE_WIDTH * CD_SCREEN_SIZE_HEIGHT * CD_PIXEL_COLOR_DEPTH) / 8;

    //cd_conv_xywh2rect(0, 0, CD_SCREEN_SIZE_WIDTH, CD_SCREEN_SIZE_HEIGHT, &rect);
    //cd_conv_xywh2rect(10, 10, 30, 30, &rect);


    void* buf_screen;
    void* prev_buf_screen;

    char filename1[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\random";
    char filename2[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\photo";
    char filename3[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\data_encoded";
    char dane_po_zdekodowaniu[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\dane_po_zdekodowaniu";
    char dane_zakodowane[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\dane_zakodowane";
    char obraz_przed_kodowaniem[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\obraz_przed_kodowaniem";
    char obraz_nowy_do_transferu[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\obraz_nowy_do_transferu";
    uint32_t filename1_size;
    uint32_t filename2_size;
    //uint32_t filename3_size;
    //uint32_t filename4_size;

    buf_screen      = alloc_read_file(filename1,    &filename1_size);
    prev_buf_screen = alloc_read_file(filename2,    &filename2_size);

    save_to_file(obraz_przed_kodowaniem, prev_buf_screen, filename2_size);
    save_to_file(obraz_nowy_do_transferu, buf_screen,     filename1_size);



// -------------------------------------------- KODOWANIE --------------------------------------------
    //ret = cd_encode(
    //    buf_screen,                   //wskaznik na caly obraz
    //    &rect,                  //prostokat ktory wskazuje jakie dane zakodowac
    //    buf_encoded,            //wsk bufora na dane zakodowane
    //    &size_buf_encoded       //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie pozakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
    //);

    ret = cd_encode2(
        prev_buf_screen,
        buf_screen,                   //wskaznik na caly obraz
        buf_encoded,            //wsk bufora na dane zakodowane
        &size_buf_encoded       //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie pozakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
    );

   
    if (ret == 0)
    {
       printf("Zakodowane dane: \n");
       // dump_frame_hex(buf_encoded, size_buf_encoded);
    }
    else
    {
        printf("Nie udalo sie zdekodowac. \n");
    }
/*
    int fd = open(filename3, O_WRONLY | O_CREAT);
    if (fd == -1) {
        printf("Error opening file %s\n", filename2);
        //return EXIT_FAILURE;
    }
    else
    {
        write(fd, buf_encoded, size_buf_encoded);
        close(fd);
    }
   */
    //memset(buf_screen, 0, size_buf_screen);

// ------------------------------------------- DEKODOWANIE -------------------------------------------
 
    save_to_file(dane_zakodowane, buf_encoded, size_buf_encoded);
    

    ret = cd_decode(
        buf_encoded,            //wsk bufora na dane zakodowane
        size_buf_encoded,       //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie pozakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
        prev_buf_screen,
        size_buf_screen
    );

    save_to_file(dane_po_zdekodowaniu, buf_screen, size_buf_screen);
    

    printf("code returned %d\n", (int)ret);
}