#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/parser.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("krakopo");
MODULE_DESCRIPTION("Disk Simulator Filesystem Module");
MODULE_VERSION("1.0");

#define DISKSIMFS_DEFAULT_MODE 0755
#define DISKSIMFS_DEFAULT_READ_DELAY 0
#define DISKSIMFS_DEFAULT_WRITE_DELAY 0

struct disksimfs_mount_opts {
	umode_t mode;
	int read_delay;
	int write_delay;
};

struct disksimfs_fs_info {
	struct disksimfs_mount_opts mount_opts;
};

/*
 * Display the mount options in /proc/mounts.
 */
static int disksimfs_show_options(struct seq_file *m, struct dentry *root)
{
	struct disksimfs_fs_info *fsi = root->d_sb->s_fs_info;


	if (fsi->mount_opts.mode != DISKSIMFS_DEFAULT_MODE)
		seq_printf(m, ",mode=%o", fsi->mount_opts.mode);

	if (fsi->mount_opts.read_delay != DISKSIMFS_DEFAULT_READ_DELAY)
		seq_printf(m, ",read_delay=%u", fsi->mount_opts.read_delay);

	if (fsi->mount_opts.write_delay != DISKSIMFS_DEFAULT_WRITE_DELAY)
		seq_printf(m, ",write_delay=%u", fsi->mount_opts.write_delay);

	return 0;
}

enum {
	Opt_mode,
	Opt_read_delay,
	Opt_write_delay,
	Opt_err
};

static const match_table_t tokens = {
	{Opt_mode, "mode=%o"},
	{Opt_read_delay, "read_delay=%u"},
	{Opt_write_delay, "write_delay=%u"},
	{Opt_err, NULL}
};

static int disksimfs_parse_options(char *data, struct disksimfs_mount_opts *opts)
{
	substring_t args[MAX_OPT_ARGS];
	int option;
	int token;
	char *p;

	opts->mode = DISKSIMFS_DEFAULT_MODE;
	opts->read_delay = DISKSIMFS_DEFAULT_READ_DELAY;
	opts->write_delay = DISKSIMFS_DEFAULT_WRITE_DELAY;

	while ((p = strsep(&data, ",")) != NULL) {
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case Opt_mode:
			if (match_octal(&args[0], &option))
				return -EINVAL;
			opts->mode = option & S_IALLUGO;
			break;
		case Opt_read_delay:
			if (match_int(&args[0], &option) || option < 0)
				return -EINVAL;
			opts->read_delay = option;
			break;
		case Opt_write_delay:
			if (match_int(&args[0], &option) || option < 0)
				return -EINVAL;
			opts->write_delay = option;
			break;
		}
	}
	return 0;
}

static const struct super_operations disksimfs_ops = {
	.statfs		= simple_statfs,
	.drop_inode = generic_delete_inode,
	.show_options	= disksimfs_show_options,
};

static struct address_space_operations disksimfs_aops = {
	.readpage	= simple_readpage,
	.write_begin	= simple_write_begin,
	.write_end	= simple_write_end,
	.set_page_dirty	= NULL, /* Set during disksimfs_init */
};

static unsigned long disksimfs_mmu_get_unmapped_area(struct file *file,
		unsigned long addr, unsigned long len, unsigned long pgoff,
		unsigned long flags)
{
	return current->mm->get_unmapped_area(file, addr, len, pgoff, flags);
}

ssize_t
disksimfs_file_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	struct disksimfs_fs_info *fsi = iocb->ki_filp->f_mapping->host->i_sb->s_root->d_sb->s_fs_info;
	mdelay(fsi->mount_opts.read_delay);
	return generic_file_read_iter(iocb, iter);
}

static ssize_t
disksimfs_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct disksimfs_fs_info *fsi = iocb->ki_filp->f_mapping->host->i_sb->s_root->d_sb->s_fs_info;
	pr_info("Delaying write for %d ms\n", fsi->mount_opts.write_delay);
	mdelay(fsi->mount_opts.write_delay);
	return generic_file_write_iter(iocb, from);
}

const struct file_operations disksimfs_file_operations = {
	.read_iter	= disksimfs_file_read_iter,
	.write_iter = disksimfs_file_write_iter,
	.mmap		= generic_file_mmap,
	.fsync		= noop_fsync,
	.splice_read	= generic_file_splice_read,
	.splice_write	= iter_file_splice_write,
	.llseek		= generic_file_llseek,
	.get_unmapped_area	= disksimfs_mmu_get_unmapped_area,
};

const struct inode_operations disksimfs_file_inode_operations = {
	.setattr	= simple_setattr,
	.getattr	= simple_getattr,
};

static const struct inode_operations disksimfs_dir_inode_operations;
 
struct inode *disksimfs_get_inode(struct super_block *sb,
				const struct inode *dir, umode_t mode, dev_t dev)
{
	struct inode * inode = new_inode(sb);

	if (inode) {
		inode->i_ino = get_next_ino();
		inode_init_owner(inode, dir, mode);
		inode->i_mapping->a_ops = &disksimfs_aops;
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_op = &disksimfs_file_inode_operations;
			inode->i_fop = &disksimfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &disksimfs_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;

			/* directory inodes start off with i_nlink == 2 (for "." entry) */
			inc_nlink(inode);
			break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			inode_nohighmem(inode);
			break;
		}
	}
	return inode;
}

static int
disksimfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
	struct inode * inode = disksimfs_get_inode(dir->i_sb, dir, mode, dev);
	int error = -ENOSPC;

	if (inode) {
		d_instantiate(dentry, inode);
		dget(dentry);	/* Extra count - pin the dentry in core */
		error = 0;
		dir->i_mtime = dir->i_ctime = current_time(dir);
	}
	return error;
}

static int disksimfs_mkdir(struct inode * dir, struct dentry * dentry, umode_t mode)
{
	int retval = disksimfs_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
		inc_nlink(dir);
	return retval;
}

static int disksimfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	return disksimfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int disksimfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname)
{
	struct inode *inode;
	int error = -ENOSPC;

	inode = disksimfs_get_inode(dir->i_sb, dir, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname)+1;
		error = page_symlink(inode, symname, l);
		if (!error) {
			d_instantiate(dentry, inode);
			dget(dentry);
			dir->i_mtime = dir->i_ctime = current_time(dir);
		} else
			iput(inode);
	}
	return error;
}

static const struct inode_operations disksimfs_dir_inode_operations = {
	.create		= disksimfs_create,
	.lookup		= simple_lookup,
	.link		= simple_link,
	.unlink		= simple_unlink,
	.symlink	= disksimfs_symlink,
	.mkdir		= disksimfs_mkdir,
	.rmdir		= simple_rmdir,
	.mknod		= disksimfs_mknod,
	.rename		= simple_rename,
};

int disksimfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct disksimfs_fs_info *fsi;
	struct inode *inode;
	int err;

	fsi = kzalloc(sizeof(struct disksimfs_fs_info), GFP_KERNEL);
	sb->s_fs_info = fsi;
	if (!fsi)
		return -ENOMEM;

	err = disksimfs_parse_options(data, &fsi->mount_opts);
	if (err)
		return err;

	sb->s_maxbytes		= MAX_LFS_FILESIZE;
	sb->s_blocksize		= PAGE_SIZE;
	sb->s_blocksize_bits	= PAGE_SHIFT;
#define DISKSIMFS_MAGIC 0xdeadbeef
	sb->s_magic		= DISKSIMFS_MAGIC;
	sb->s_op		= &disksimfs_ops;
	sb->s_time_gran		= 1;

	inode = disksimfs_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
	sb->s_root = d_make_root(inode);
	if (!sb->s_root)
		return -ENOMEM;

	return 0;

}

static struct dentry *disksimfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_nodev(fs_type, flags, data, disksimfs_fill_super);
}

static struct file_system_type disksimfs_type = {
	.owner		= THIS_MODULE,
	.name		= "disksimfs",
	.mount		= disksimfs_mount,
	.kill_sb	= kill_litter_super,
	.fs_flags	= FS_USERNS_MOUNT,
};

static int __init disksimfs_init(void) {
	int error;

	error = register_filesystem(&disksimfs_type);
	if (error) {
		pr_err("Could not register disksimfs\n");
		return error;
	}

	disksimfs_aops.set_page_dirty = (int (*)(struct page *)) kallsyms_lookup_name("__set_page_dirty_no_writeback");

	return 0;
}

static void __exit disksimfs_exit(void) {
	unregister_filesystem(&disksimfs_type);
}

module_init(disksimfs_init);
module_exit(disksimfs_exit);
