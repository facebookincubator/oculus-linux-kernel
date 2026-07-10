#include "oom_kill_stats.h"

#define K(x) ((x) << (PAGE_SHIFT - 10))
#define MAX_VICTIMS 10

struct victim_info {
	char process_name[TASK_COMM_LEN];
	long time;
	unsigned long total_vm;
	unsigned long anon_rss;
	unsigned long file_rSS;
	unsigned long shmem_rSS;
	short oom_score_adj;
};

static struct victim_info victims[MAX_VICTIMS];
static int victims_index = 0;
static bool is_overflow = false;

DEFINE_MUTEX(oom_kill_stats_lock);

void add_victim(struct task_struct *victim)
{
	mutex_lock(&oom_kill_stats_lock);

	// overwrite the oldest entry
	if (victims_index == MAX_VICTIMS) {
		is_overflow = true;
		victims_index = 0;
	}

	strncpy(victims[victims_index].process_name, victim->comm,
		TASK_COMM_LEN);
	victims[victims_index].process_name[TASK_COMM_LEN - 1] = '\0';
	victims[victims_index].time = ktime_get_real_seconds();
	victims[victims_index].total_vm = K(victim->mm->total_vm);
	victims[victims_index].anon_rss =
		K(get_mm_counter(victim->mm, MM_ANONPAGES));
	victims[victims_index].file_rSS =
		K(get_mm_counter(victim->mm, MM_FILEPAGES));
	victims[victims_index].shmem_rSS =
		K(get_mm_counter(victim->mm, MM_SHMEMPAGES));
	victims[victims_index].oom_score_adj = victim->signal->oom_score_adj;

	victims_index++;

	mutex_unlock(&oom_kill_stats_lock);
}

void dump_victims(struct seq_file *file, bool deleteAfterDump)
{
	int i;

	mutex_lock(&oom_kill_stats_lock);

	if (is_overflow)
		for (i = victims_index; i < MAX_VICTIMS; i++)
			seq_printf(file, "%s\t%ld\t%lu\t%lu\t%lu\t%lu\t%hd\n",
				   victims[i].process_name, victims[i].time,
				   victims[i].total_vm, victims[i].anon_rss,
				   victims[i].file_rSS, victims[i].shmem_rSS,
				   victims[i].oom_score_adj);

	for (i = 0; i < victims_index; i++)
		seq_printf(file, "%s\t%ld\t%lu\t%lu\t%lu\t%lu\t%hd\n",
			   victims[i].process_name, victims[i].time,
			   victims[i].total_vm, victims[i].anon_rss,
			   victims[i].file_rSS, victims[i].shmem_rSS,
			   victims[i].oom_score_adj);

	// do not print anything if there are no victims
	if (victims_index || is_overflow)
		seq_printf(file, "%d", is_overflow);

	if (deleteAfterDump) {
		victims_index = 0;
		is_overflow = false;
	}

	mutex_unlock(&oom_kill_stats_lock);
}
