udf_find_last(struct super_block *sb)
{
	long value=-1;
  	struct inode inode_fake;
	int i,j;

  	if (get_blkfops(MAJOR(sb->s_dev))->ioctl!=NULL) {
      		/* Whoops.  We must save the old FS, since otherwise
       		 * we would destroy the kernels idea about FS on root
       		 * mount in read_super... [chexum]
       		 */
		struct cdrom_tochdr toc_h;
		struct cdrom_tocentry toc_e;
      		mm_segment_t old_fs=get_fs();

      		inode_fake.i_rdev=sb->s_dev;
      		set_fs(KERNEL_DS);

      		i=get_blkfops(MAJOR(sb->s_dev))->ioctl(&inode_fake,
				       NULL,
				       BLKGETSIZE,
				       (unsigned long) &value);
      		set_fs(old_fs);
		if ( i == 0 ) {
			printk(KERN_DEBUG "udf: ioctl(BLKGETSIZE), v=%d\n",
				value);
		} else {
			value=-1;
			/* check for cdrom tracks */
      			set_fs(KERNEL_DS);

      			i=get_blkfops(MAJOR(sb->s_dev))->ioctl(&inode_fake,
				       NULL,
				       CDROMREADTOCHDR,
				       (unsigned long) &toc_h);
      			set_fs(old_fs);
			if ( i == 0 ) {
			    printk(KERN_DEBUG "udf: cdtracks first: %u last: %u\n",
					toc_h.cdth_trk0, toc_h.cdth_trk1);
			    /* check the first cdrom track */
			    for (j=toc_h.cdth_trk0; j<= toc_h.cdth_trk1; j++) {
				toc_e.cdte_track = j;
				toc_e.cdte_format = CDROM_LBA;
      				set_fs(KERNEL_DS);

      				i=get_blkfops(
					MAJOR(sb->s_dev))->ioctl(&inode_fake,
					NULL,
				        CDROMREADTOCENTRY,
				        (unsigned long) &toc_e);
      				set_fs(old_fs);
				if ( i == 0 ) {
					printk(KERN_DEBUG "udf: cd track(%d): ctrl %x last: %u\n",
						j, toc_e.cdte_ctrl,
						toc_e.cdte_addr.lba);
				}
			    } /* end for */
				j=0xAA; /* leadout track */
				toc_e.cdte_track = j;
				toc_e.cdte_format = CDROM_LBA;
      				set_fs(KERNEL_DS);

      				i=get_blkfops(
					MAJOR(sb->s_dev))->ioctl(&inode_fake,
					NULL,
				        CDROMREADTOCENTRY,
				        (unsigned long) &toc_e);
      				set_fs(old_fs);
				if ( i == 0 ) {
					printk(KERN_DEBUG "udf: cd track(%d): ctrl %x start: %u\n",
						j, toc_e.cdte_ctrl,
						toc_e.cdte_addr.lba);
				}
			}
		}
	}
	return value;
}
