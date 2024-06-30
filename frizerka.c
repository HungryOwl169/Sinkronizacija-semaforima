#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <semaphore.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>

#define RADNO_VRIJEME 10
#define N 3
#define BROJ_KLIJENATA 7

int id;
struct zajednicke_varijable
{
    int kraj_radnog_vremena;
    int trenutni_klijent;
    int otvoreno;
    int br_mjesta;
    int postavljen;
    // sem_t cekaonica;
    sem_t stolac;
    sem_t trenutni_klijent_sem;
    sem_t otvoreno_sem;
    sem_t br_mjesta_sem;
    sem_t kraj_radnog_vremena_sem;
    sem_t sjeo_sam_sem;
    sem_t budi_se_sem;
    sem_t pocela_frizura;
    sem_t postavljen_sem;
};
struct zajednicke_varijable *varijable;

void brisi(void)
{

    /* oslobađanje zajedničke memorije */

    (void)shmdt((struct zajednicke_varijable *)varijable);

    (void)shmctl(id, IPC_RMID, NULL);

    return;
}

void postavi_radno_vrijeme(int vrijeme)
{
    sleep(vrijeme);
    sem_wait(&varijable->kraj_radnog_vremena_sem);
    varijable->kraj_radnog_vremena = 1;
    sem_post(&varijable->kraj_radnog_vremena_sem);
    sem_post(&varijable->budi_se_sem);
    // printf("Postavi_radno_vrijeme je gotovo\n");
    return;
}

void frizerka()
{
    printf("Frizerka: Otvaram salon\n");
    sem_wait(&varijable->otvoreno_sem);
    printf("Frizerka: Postavljam znak OTVORENO\n");
    varijable->otvoreno = 1;
    sem_post(&varijable->otvoreno_sem);

    while (1)
    {
        sem_wait(&varijable->kraj_radnog_vremena_sem);
        int curr_radno_vrijeme = varijable->kraj_radnog_vremena;
        sem_post(&varijable->kraj_radnog_vremena_sem);

        sem_wait(&varijable->br_mjesta_sem);
        int curr_br_mjesta = varijable->br_mjesta;
        sem_post(&varijable->br_mjesta_sem);

        if (curr_radno_vrijeme == 1)
        {
            sem_wait(&varijable->otvoreno_sem);
            printf("Frizerka: Postavljam znak ZATVORENO\n");
            varijable->otvoreno = 0;
            sem_post(&varijable->otvoreno_sem);
            //  printf("Frizerka: Tu sam 1\n");
        }
        if (curr_br_mjesta != N) // ako ima klijenata
        {
            sem_post(&varijable->stolac);
            sem_wait(&varijable->sjeo_sam_sem);
            sem_wait(&varijable->trenutni_klijent_sem);
            printf("Frizerka: Idem raditi na klijentu %d\n", varijable->trenutni_klijent);
            sem_post(&varijable->pocela_frizura);
            sleep(1); // radi frizuru
            printf("Frizerka: Klijent %d gotov\n", varijable->trenutni_klijent);
            sem_post(&varijable->trenutni_klijent_sem);

            //  printf("Frizerka: Tu sam 2\n");
        }
        else if (curr_radno_vrijeme == 0) // nema nikog ali nije kraj
        {
            printf("Frizerka: Spavam dok klijenti ne dodu\n");
            sem_wait(&varijable->budi_se_sem);
            // printf("Frizerka: Tu sam 3\n");
        }
        else // nema nikog i kraj je
        {
            printf("Frizerka: Zatvaram salon\n");
            //  printf("Frizerka: Tu sam 4\n");
            return;
        }
    }
}

void klijent(int x)
{
    printf("Klijent(%d): Zelim na frizuru\n", x);

    sem_wait(&varijable->otvoreno_sem);
    int curr_otvoreno = varijable->otvoreno;
    sem_post(&varijable->otvoreno_sem);

    //  printf("Klijent: Tu sam 1 i otvoreno = %d\n", curr_otvoreno);

    sem_wait(&varijable->br_mjesta_sem);
    int curr_broj_mjesta = varijable->br_mjesta;
    sem_post(&varijable->br_mjesta_sem);

    //  printf("Klijent: Tu sam 2 i broj mjesta = %d\n", curr_broj_mjesta);

    if (curr_otvoreno == 1 && curr_broj_mjesta != 0)
    {
        sem_wait(&varijable->br_mjesta_sem);
        varijable->br_mjesta--;
        printf("Klijent(%d) Ulazim u cekaonicu (%d)\n", x, N - varijable->br_mjesta);
        sem_post(&varijable->br_mjesta_sem);

        sem_wait(&varijable->postavljen_sem);
        if (varijable->postavljen == 0)
        {
            sem_post(&varijable->budi_se_sem);
            varijable->postavljen = 1;
        }
        sem_post(&varijable->postavljen_sem);

        //sem_post(&varijable->budi_se_sem);

        // printf("Klijent: Tu sam 3\n");

        // sem_post(&varijable->cekaonica);
        sem_wait(&varijable->stolac);

        //  printf("Klijent: Tu sam 4\n");

        sem_wait(&varijable->br_mjesta_sem);
        varijable->br_mjesta++;
        // printf("Klijent: Postavio sam broj mjesta na %d\n", varijable->br_mjesta);
        sem_post(&varijable->br_mjesta_sem);

        //  printf("Klijent: Tu sam 5\n");

        sem_wait(&varijable->trenutni_klijent_sem);
        varijable->trenutni_klijent = x;
        // printf("Klijent: Trenutni klijent = %d\n", varijable->trenutni_klijent);
        sem_post(&varijable->trenutni_klijent_sem);
        sem_post(&varijable->sjeo_sam_sem);

        sem_wait(&varijable->pocela_frizura);
        printf("Klijent(%d): Frizerka mi radi frizuru\n", x);
    }
    else
    {
        printf("Klijent(%d): Nema mjesta u cekaoni, vratit cu se sutra\n", x);
        //  printf("Frizerka: Tu sam 6\n");
    }
}

int main(void)
{
    // zauzimanje memorije
    id = shmget(IPC_PRIVATE, sizeof(struct zajednicke_varijable), 0600);

    if (id == -1)
        exit(1);

    varijable = (struct zajednicke_varijable *)shmat(id, NULL, 0);

    // INICIJALIZACIJA
    // varijable:
    varijable->br_mjesta = N;
    varijable->otvoreno = 0;
    varijable->kraj_radnog_vremena = 0;
    varijable->postavljen = 0;

    // semafori:
    sem_init(&varijable->br_mjesta_sem, 1, 1);
    sem_init(&varijable->otvoreno_sem, 1, 1);
    sem_init(&varijable->kraj_radnog_vremena_sem, 1, 1);
    sem_init(&varijable->trenutni_klijent_sem, 1, 1);
    sem_init(&varijable->stolac, 1, 0);
    // sem_init(&varijable->cekaonica, 1, 0); // sem_init (sem, 1, 5); //početna vrijednost = 5, 1=>za procese
    sem_init(&varijable->sjeo_sam_sem, 1, 0);
    sem_init(&varijable->budi_se_sem, 1, 0);
    sem_init(&varijable->pocela_frizura, 1, 0);
    sem_init(&varijable->postavljen_sem, 1, 1);

    if (fork() == 0)
    {
        frizerka();
        exit(0);
    }

    if (fork() == 0)
    {
        postavi_radno_vrijeme(RADNO_VRIJEME);
        exit(0);
    }

    sleep(2);

    for (int i = 1; i <= BROJ_KLIJENATA; i++)
    {
        if (i >= 6)
            sleep(1);
        if (fork() == 0)
        {
            klijent(i);
            exit(0);
        }
    }

    brisi();

    int broj_procesa = BROJ_KLIJENATA + 2;
    for (int i = 1; i <= broj_procesa; i++)
        (void)wait(NULL);

    return 0;
}
