#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/sysinfo.h>

// Määritellään, kuinka suuren osan tiedostosta kukin säie käsittelee
#define CHUNK_SIZE 1024

// Rakenteet säikeiden argumentteihin
typedef struct {
    char *start;       // Osoitin tiedoston alkuun kyseisessä säikeessä
    size_t size;       // Käsiteltävän osan koko
    FILE *output;      // Tiedosto, johon pakattu data kirjoitetaan
    pthread_mutex_t *mutex; // Mutex-tiedosto kirjoittamisen suojaamiseksi
} thread_args_t;

// Mutex-suojaus tulostamiselle
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Funktion, joka hoitaa pakkaamisen
void *compress_chunk(void *arg) {
    thread_args_t *args = (thread_args_t *)arg; // Muunnetaan argumentti oikeaksi rakenteeksi
    char *start = args->start;   // Alkuosa tiedostosta
    size_t size = args->size;   // Käsiteltävän osan koko
    FILE *output = args->output; // Tiedosto, johon pakattu data kirjoitetaan
    pthread_mutex_t *mutex = args->mutex; // Mutex kirjoittamisen suojaamiseksi

    // RLE (Run-Length Encoding) pakkaus
    size_t i = 0;
    while (i < size) {
        char current_char = start[i];  // Nykyinen merkki
        size_t run_length = 1;         // Alustetaan toistojen määrä

        // Laske, kuinka monta kertaa nykyinen merkki toistuu peräkkäin
        while (i + run_length < size && start[i + run_length] == current_char) {
            run_length++;
        }

        // Suojataan tulostaminen mutexilla, jotta vain yksi säie voi kirjoittaa kerrallaan
        pthread_mutex_lock(mutex);
        fprintf(output, "%c%zu", current_char, run_length); // Kirjoitetaan merkki ja sen toistomäärä
        pthread_mutex_unlock(mutex);

        i += run_length; // Siirrytään seuraavaan merkkiin
    }

    pthread_exit(NULL); // Lopetetaan säie
}

int main(int argc, char *argv[]) {
    // Tarkistetaan, että syöte-tiedosto on annettu
    if (argc < 2) {
        fprintf(stderr, "Käyttö: %s <file1> [file2 ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_files = argc - 1; // Syöte-tiedostojen määrä
    pthread_t *threads = malloc(num_files * sizeof(pthread_t)); // Säikeiden id:it
    thread_args_t *thread_args = malloc(num_files * sizeof(thread_args_t)); // Säikeiden argumentit
    FILE *output_file = fopen("file.z", "w"); // Tulostiedosto
    if (!output_file) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    // Käydään läpi kaikki syöte-tiedostot ja luodaan säikeet niiden käsittelemiseksi
    for (int file_idx = 0; file_idx < num_files; file_idx++) {
        int fd = open(argv[file_idx + 1], O_RDONLY); // Avaa syöte-tiedosto
        if (fd == -1) {
            perror("open");
            return EXIT_FAILURE;
        }

        off_t file_size = lseek(fd, 0, SEEK_END); // Hanki tiedoston koko
        char *file_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0); // Mappaa tiedosto muistiin
        if (file_data == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return EXIT_FAILURE;
        }
        close(fd);

        size_t chunk_size = file_size / num_files; // Lasketaan chunkin koko (tiedoston koko / säikeiden määrä)
        thread_args[file_idx].start = file_data; // Asetetaan säikeen tiedoston aloitusosoite
        thread_args[file_idx].size = file_size; // Asetetaan säikeen käsiteltävän osan koko
        thread_args[file_idx].output = output_file; // Asetetaan säikeelle tulostiedosto
        thread_args[file_idx].mutex = &print_mutex; // Asetetaan säikeelle mutex

        pthread_create(&threads[file_idx], NULL, compress_chunk, &thread_args[file_idx]); // Luo säie
    }

    // Odotetaan, että kaikki säikeet ovat valmiit
    for (int i = 0; i < num_files; i++) {
        pthread_join(threads[i], NULL);
    }

    // Vapauta varatut resurssit
    fclose(output_file); // Sulje tulostiedosto
    free(threads);       // Vapauta säikeiden muisti
    free(thread_args);   // Vapauta säikeiden argumenttien muisti
    pthread_mutex_destroy(&print_mutex); // Tuhoa mutex

    return EXIT_SUCCESS; // Ohjelman onnistunut lopetus
}
