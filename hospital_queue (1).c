/*
 * Hospital Queue Management System for Patient Flow Optimization (C version)
 * --------------------------------------------------------------------------
 * - Patients get a token number and a triage priority:
 *       1 = Emergency, 2 = Urgent, 3 = Normal
 * - The waiting list is a linked list kept sorted by priority; patients with
 *   the same priority stay in arrival order (FIFO).
 * - Several doctors work in parallel; the next patient goes to a free doctor.
 * - Estimated waiting time and wait statistics are reported.
 * - Completed visits are appended to visit_log.csv.
 *
 * Compile:  gcc hospital_queue.c -o hospital_queue
 * Run:      ./hospital_queue        (hospital_queue.exe on Windows)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_DOCTORS 3

typedef struct Patient {
    int token;
    char name[50];
    int age;
    char complaint[80];
    int priority;          /* 1, 2 or 3 */
    double arrival;        /* minutes since program start */
    double start;          /* consultation start time */
    struct Patient *next;
} Patient;

static const char *PRIORITY_NAME[] = {"", "Emergency", "Urgent", "Normal"};
static const int AVG_CONSULT[] = {0, 20, 15, 10};          /* minutes */
static const char *DOCTOR_NAME[NUM_DOCTORS] = {"Dr.Rao", "Dr.Priya", "Dr.Kumar"};

static Patient *head = NULL;                    /* waiting queue */
static Patient *inRoom[NUM_DOCTORS] = {NULL};   /* patient with each doctor */
static int nextToken = 1;
static time_t startTime;

/* statistics, index 0 = overall, 1..3 = by priority */
static int    servedCount[4] = {0};
static double waitTotal[4]   = {0};
static double waitMax        = 0;

static double now(void) { return difftime(time(NULL), startTime) / 60.0; }

/* ---------- input helpers ---------- */
static void readLine(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    if (!fgets(buf, size, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
}

static int readInt(const char *prompt, int low, int high) {
    char line[50];
    int v;
    for (;;) {
        printf("%s", prompt);
        if (!fgets(line, sizeof line, stdin)) return low;
        if (sscanf(line, "%d", &v) == 1 && v >= low && v <= high) return v;
        printf("  Enter a number between %d and %d.\n", low, high);
    }
}

/* ---------- queue operations ---------- */
static int estimatedWait(const Patient *target) {
    int total = 0;
    for (const Patient *p = head; p && p != target; p = p->next)
        total += AVG_CONSULT[p->priority];
    return total / NUM_DOCTORS;
}

static void registerPatient(void) {
    Patient *p = (Patient *)malloc(sizeof(Patient));
    if (!p) { printf("Out of memory.\n"); return; }

    readLine("Name: ", p->name, sizeof p->name);
    p->age = readInt("Age: ", 0, 120);
    readLine("Complaint: ", p->complaint, sizeof p->complaint);
    p->priority = readInt("Priority (1=Emergency 2=Urgent 3=Normal): ", 1, 3);
    p->token = nextToken++;
    p->arrival = now();
    p->start = 0;
    p->next = NULL;

    /* insert after every patient with the same or higher priority */
    if (head == NULL || p->priority < head->priority) {
        p->next = head;
        head = p;
    } else {
        Patient *cur = head;
        while (cur->next && cur->next->priority <= p->priority)
            cur = cur->next;
        p->next = cur->next;
        cur->next = p;
    }
    printf("Token #%d issued. Estimated wait: %d min\n", p->token, estimatedWait(p));
}

static void callNext(void) {
    if (head == NULL) { printf("No patients waiting.\n"); return; }

    int d = -1;
    for (int i = 0; i < NUM_DOCTORS; i++)
        if (inRoom[i] == NULL) { d = i; break; }
    if (d < 0) { printf("All doctors are busy.\n"); return; }

    Patient *p = head;
    head = head->next;
    p->next = NULL;
    p->start = now();
    inRoom[d] = p;

    double w = p->start - p->arrival;
    servedCount[0]++;           servedCount[p->priority]++;
    waitTotal[0] += w;          waitTotal[p->priority] += w;
    if (w > waitMax) waitMax = w;

    printf("Token #%d %s -> %s (%s, waited %.1f min)\n",
           p->token, p->name, DOCTOR_NAME[d], PRIORITY_NAME[p->priority], w);
}

static void finishConsultation(void) {
    int busy = 0;
    for (int i = 0; i < NUM_DOCTORS; i++)
        if (inRoom[i]) { printf("  %d. %s with %s\n", i + 1, DOCTOR_NAME[i], inRoom[i]->name); busy++; }
    if (!busy) { printf("No doctor is currently consulting.\n"); return; }

    int d = readInt("Doctor number: ", 1, NUM_DOCTORS) - 1;
    Patient *p = inRoom[d];
    if (!p) { printf("%s is already free.\n", DOCTOR_NAME[d]); return; }

    FILE *f = fopen("visit_log.csv", "a");
    if (f) {
        fseek(f, 0, SEEK_END);
        if (ftell(f) == 0)
            fprintf(f, "Token,Name,Age,Complaint,Priority,Doctor,Arrival,Start,End,Wait\n");
        fprintf(f, "%d,%s,%d,%s,%s,%s,%.1f,%.1f,%.1f,%.1f\n",
                p->token, p->name, p->age, p->complaint, PRIORITY_NAME[p->priority],
                DOCTOR_NAME[d], p->arrival, p->start, now(), p->start - p->arrival);
        fclose(f);
    }
    printf("%s finished with %s. %s is free.\n", p->name, DOCTOR_NAME[d], DOCTOR_NAME[d]);
    free(p);
    inRoom[d] = NULL;
}

static void viewQueue(void) {
    if (!head) printf("\nQueue is empty.\n");
    else printf("\nWaiting queue:\n");
    int pos = 1;
    for (Patient *p = head; p; p = p->next, pos++)
        printf(" %d. #%d %-12s %-10s est. wait %d min\n",
               pos, p->token, p->name, PRIORITY_NAME[p->priority], estimatedWait(p));
    for (int i = 0; i < NUM_DOCTORS; i++) {
        if (inRoom[i]) printf(" %s: with %s\n", DOCTOR_NAME[i], inRoom[i]->name);
        else           printf(" %s: free\n", DOCTOR_NAME[i]);
    }
}

static void showStats(void) {
    int waiting = 0;
    for (Patient *p = head; p; p = p->next) waiting++;
    printf("\nPatients called so far : %d\n", servedCount[0]);
    printf("Patients still waiting : %d\n", waiting);
    for (int i = 0; i <= 3; i++) {
        double avg = servedCount[i] ? waitTotal[i] / servedCount[i] : 0;
        printf("Average wait (%-9s): %.1f min\n", i == 0 ? "Overall" : PRIORITY_NAME[i], avg);
    }
    printf("Longest wait           : %.1f min\n", waitMax);
}

static void freeAll(void) {
    while (head) { Patient *t = head; head = head->next; free(t); }
    for (int i = 0; i < NUM_DOCTORS; i++) { free(inRoom[i]); inRoom[i] = NULL; }
}

int main(void) {
    startTime = time(NULL);
    for (;;) {
        printf("\n===== HOSPITAL QUEUE MANAGEMENT =====\n");
        printf("1. Register patient      2. Call next patient\n");
        printf("3. Finish consultation   4. View queue\n");
        printf("5. Statistics            0. Exit\n");
        int choice = readInt("Choose: ", 0, 5);
        switch (choice) {
            case 1: registerPatient();     break;
            case 2: callNext();            break;
            case 3: finishConsultation();  break;
            case 4: viewQueue();           break;
            case 5: showStats();           break;
            case 0: freeAll(); printf("Goodbye!\n"); return 0;
        }
    }
}
