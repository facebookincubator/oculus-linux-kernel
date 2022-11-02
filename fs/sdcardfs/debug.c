#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/rwsem.h>
#include <linux/seq_file.h>
#include "sdcardfs.h"

// tokenizer state for tokenzing start/stop tracking commands
enum track_token {
	TOK_NONE,
	TOK_START,
	TOK_PID,
};

enum io_op {
	OP_READ,
	OP_WRITE,
};

struct log_entry {
	struct log_entry *next;
	ktime_t ts;
	pid_t pid;
	loff_t offset;
	size_t length;
	enum io_op op;
};

struct track_entry {
	struct list_head list;
	struct log_entry *log;
	struct log_entry **log_tail;
	struct rw_semaphore log_lock;
	struct qstr name;
	pid_t pid;
};

struct sdcardfs_debug_data {
	struct dentry *debug_root;
	struct rw_semaphore list_lock;
	struct list_head tracked_list;
};

static struct dentry *sdcardfs_debug_root;

/*
 * Checks if debug tracing is enabled for the specified file from the current process
 *
 * Returns the tracking entry with the log to update, if the access should be logged
 */
static struct track_entry *_is_tracked_access(struct sdcardfs_debug_data *dd, struct dentry *dentry)
{
	pid_t tgid = task_tgid_nr(current);
	struct track_entry *te;

	list_for_each_entry(te, &dd->tracked_list, list) {
		if ((te->pid == -1 || te->pid == tgid) && qstr_case_eq(&dentry->d_name, &te->name))
			return te;
	}

	return NULL;
}

static const char *_name_for_op(enum io_op op)
{
	switch (op) {
	case OP_READ: return "r";
	case OP_WRITE: return "w";
	default: return "?";
	}
}

static int log_file_show(struct seq_file *s, void *unused)
{
	struct sdcardfs_debug_data *dd = s->private;
	struct track_entry *te;

	seq_printf(s, "%12s\t%64s\t%8s\t%3s\t%12s\t%10s\n", "usec", "file", "pid", "op", "offset", "length");
	down_read(&dd->list_lock);
	list_for_each_entry(te, &dd->tracked_list, list) {
		struct log_entry *le;

		down_read(&te->log_lock);
		for (le = te->log; le; le = le->next) {
			u64 usec = ktime_to_us(le->ts);
			seq_printf(s, "%12llu\t%64s\t%8d\t%3s\t%12lu\t%10zu\n", usec, te->name.name, le->pid,
				   _name_for_op(le->op), le->offset, le->length);
		}
		up_read(&te->log_lock);
	}
	up_read(&dd->list_lock);
	return 0;
}

static int track_file_show(struct seq_file *s, void *unused)
{
	struct sdcardfs_debug_data *dd = s->private;
	struct track_entry *te;
	int count = 0;

	down_read(&dd->list_lock);
	list_for_each_entry(te, &dd->tracked_list, list) {
		seq_printf(s, "%10d %s\n", te->pid, te->name.name);
		count++;
	}
	up_read(&dd->list_lock);
	seq_printf(s, "Tracking %d items\n", count);
	return 0;
}

static inline int is_sep(char c)
{
	return isspace(c) || !c;
}

static inline void free_tracked_entry(struct track_entry *te)
{
	down_write(&te->log_lock);
	while (te->log) {
		struct log_entry *le;

		le = te->log;
		te->log = le->next;
		kfree(le);
	}
	te->log_tail = &te->log;
	up_write(&te->log_lock);
	kfree(te->name.name);
	kfree(te);
}

/*
 * Tokenizes user input of the form:
 *
 * [start|stop] [pid|*] [filename]
 *
 * to create a list of files that should have their IO accesses tracked at runtime
 */
static ssize_t track_file_write(struct file *file, const char __user *buf,
				size_t count, loff_t *offs)
{
	struct sdcardfs_debug_data *dd = ((struct seq_file *)file->private_data)->private;
	char *page = (char *)__get_free_page(GFP_KERNEL);
	enum track_token state = TOK_NONE;
	ssize_t copied = 0;
	size_t remain = 0;
	pid_t tracked_pid;
	int is_add;

	if (!page)
		return -ENOMEM;

	while (count > 0 || remain > 0) {
		size_t chunk = min(PAGE_SIZE - remain, count);
		size_t consumed = 0, token_end = 0;

		if (chunk && copy_from_user(page + remain, buf, chunk)) {
			copied = -EFAULT;
			break;
		}

		count -= chunk;
		buf += chunk;
		chunk += remain;

		// swallow all leading separators
		while (consumed < chunk && is_sep(page[consumed]))
			consumed++;

		if (consumed == chunk)
			goto loop;

		// advance to the next separator character
		for (token_end = consumed; token_end < chunk && !is_sep(page[token_end]); )
			token_end++;

		// didn't encounter a separator in the remaining data, grab more data
		if (token_end == chunk && count)
			goto loop;

		switch (state) {
		case TOK_NONE:
			if (!strncmp("start", page + consumed, token_end - consumed)) {
				is_add = 1;
				tracked_pid = 0;
				state = TOK_START;
			} else if (!strncmp("stop", page + consumed, token_end - consumed)) {
				is_add = 0;
				tracked_pid = 0;
				state = TOK_START;
			} else {
				copied = -EINVAL;
				goto out;
			}
			break;
		case TOK_START:
			if (token_end - consumed == 1 && page[consumed] == '*') {
				tracked_pid = -1;
				state = TOK_PID;
			} else if (sscanf(page + consumed, "%d", &tracked_pid) == 1) {
				state = TOK_PID;
			} else {
				copied = -EINVAL;
				goto out;
			}
			break;
		case TOK_PID:
			if (is_add) {
				struct track_entry *te = kzalloc(sizeof(*te), GFP_KERNEL);
				char *name = kstrndup(page + consumed, token_end - consumed, GFP_KERNEL);

				if (te && name) {
					struct qstr q = QSTR_INIT(name, token_end - consumed);

					te->pid = tracked_pid;
					te->name = q;
					te->log = NULL;
					te->log_tail = &te->log;
					init_rwsem(&te->log_lock);
					down_write(&dd->list_lock);
					list_add_tail(&te->list, &dd->tracked_list);
					up_write(&dd->list_lock);
				} else {
					kfree(te);
					kfree(name);
					copied = -ENOMEM;
					goto out;
				}
			} else {
				struct track_entry *te;
				char *name = kstrndup(page + consumed, token_end - consumed, GFP_KERNEL);
				struct qstr q = QSTR_INIT(name, token_end - consumed);
				int found = 0;

				down_write(&dd->list_lock);
				list_for_each_entry(te, &dd->tracked_list, list) {
					if (te->pid == tracked_pid && qstr_case_eq(&q, &te->name)) {
						list_del(&te->list);
						found = 1;
						break;
					}
				}
				up_write(&dd->list_lock);
				kfree(name);
				if (found)
					free_tracked_entry(te);
			}
			state = TOK_NONE;
			break;
		}

		consumed = token_end;

		// swallow all trailing separators
		while (consumed < chunk && is_sep(page[consumed]))
			consumed++;

loop:
		remain = chunk - consumed;
		copied += consumed;
		if (remain)
			memmove(page, page + consumed, remain);
	}

out:
	free_page((unsigned long) page);
	return copied;
}

static int track_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, track_file_show, inode->i_private);
}

static int log_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, log_file_show, inode->i_private);
}

static const struct file_operations tracked_fops = {
	.open = track_file_open,
	.read = seq_read,
	.write = track_file_write,
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.release = single_release,
};

static const struct file_operations log_fops = {
	.open = log_file_open,
	.read = seq_read,
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.release = single_release,
};

void sdcardfs_debug_log_read(struct dentry *dentry, loff_t pos, size_t len)
{
	struct sdcardfs_sb_info *sbi = SDCARDFS_SB(dentry->d_sb);
	struct sdcardfs_debug_data *dd = sbi->debug_data;
	struct track_entry *te;
	struct log_entry *le;

	if (!dd)
		return;

	down_read(&dd->list_lock);
	te = _is_tracked_access(dd, dentry);
	if (te)
		down_write(&te->log_lock);
	up_read(&dd->list_lock);

	if (!te)
		return;

	le = kzalloc(sizeof(*le), GFP_KERNEL);
	if (!le)
		goto unlock;

	le->offset = pos;
	le->length = len;
	le->ts = ktime_get_boottime();
	le->op = OP_READ;
	le->pid = task_tgid_nr(current);
	*te->log_tail = le;
	te->log_tail = &le->next;

unlock:
	up_write(&te->log_lock);
}

int sdcardfs_sb_debug_init(struct sdcardfs_sb_info *spd)
{
	struct sdcardfs_debug_data *dd;
	char debug_name[32];
	char buf[256];

	if (!sdcardfs_debug_root) {
		pr_err("sdcardfs: debugfs not initialized\n");
		goto out;
	}
	dd = kzalloc(sizeof(*dd), GFP_KERNEL);
	if (!dd) {
		pr_err("sdcardfs: unable to initialize debug data\n");
		goto out;
	}

	snprintf(debug_name, sizeof(debug_name), "%lu", spd->sb->s_root->d_inode->i_ino);
	dd->debug_root = debugfs_create_dir(debug_name, sdcardfs_debug_root);
	if (IS_ERR_OR_NULL(dd->debug_root)) {
		pr_err("sdcardfs: unable to setup debug directory for %s\n", debug_name);
		goto out_alloc;
	}
	if (IS_ERR_OR_NULL(debugfs_create_file("tracked", 0664, dd->debug_root, dd, &tracked_fops))) {
		pr_err("sdcardfs: failed to create %s/%s\n", dentry_path(dd->debug_root, buf, sizeof(buf)), "tracked");
		goto out_debugfs;
	}
	if (IS_ERR_OR_NULL(debugfs_create_file("log", 0444, dd->debug_root, dd, &log_fops))) {
		pr_err("sdcardfs: failed to create %s/%s\n", dentry_path(dd->debug_root, buf, sizeof(buf)), "log");
		goto out_debugfs;
	}
	init_rwsem(&dd->list_lock);
	INIT_LIST_HEAD(&dd->tracked_list);
	spd->debug_data = dd;
	return 0;

out_debugfs:
	debugfs_remove_recursive(dd->debug_root);
out_alloc:
	kfree(dd);
out:
	return -1;
}

void sdcardfs_sb_debug_destroy(struct sdcardfs_sb_info *spd)
{
	struct sdcardfs_debug_data *dd = spd->debug_data;
	struct track_entry *te;
	LIST_HEAD(head);

	if (!dd)
		return;

	down_write(&dd->list_lock);
	while (!list_empty(&dd->tracked_list)) {
		te = list_first_entry(&dd->tracked_list, struct track_entry, list);
		list_del(&te->list);
		list_add_tail(&te->list, &head);
	}
	up_write(&dd->list_lock);
	while (!list_empty(&head)) {
		te = list_first_entry(&head, struct track_entry, list);
		list_del(&te->list);
		free_tracked_entry(te);
	}
	debugfs_remove_recursive(dd->debug_root);
}

int sdcardfs_debug_init(void)
{
	struct dentry *dentry;

	dentry = debugfs_create_dir("sdcardfs", NULL);
	sdcardfs_debug_root = IS_ERR(dentry) ? NULL : dentry;
	return PTR_ERR_OR_ZERO(dentry);
}
