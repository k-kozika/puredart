static int probe_value = 23;
void *shared_pointer = &probe_value;
extern int external_value;
void *external_pointer = &external_value;
struct Sample {
    int pad;
    long hooktools_member;
} bss_sample;
int probe_function(void) { return probe_value; }
