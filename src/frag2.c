isofs_get_last_session(kdev_t dev)
{
  struct cdrom_multisession ms_info;
  unsigned int vol_desc_start;
  struct inode inode_fake;
  int i;

  vol_desc_start=0;
  if (get_blkfops(MAJOR(dev))->ioctl!=NULL) {
      /* Whoops.  We must save the old FS, since otherwise
       * we would destroy the kernels idea about FS on root
       * mount in read_super... [chexum]
       */
      mm_segment_t old_fs=get_fs();
      inode_fake.i_rdev=dev;
      ms_info.addr_format=CDROM_LBA;
      set_fs(KERNEL_DS);
      i=get_blkfops(MAJOR(dev))->ioctl(&inode_fake,
				       NULL,
				       CDROMMULTISESSION,
				       (unsigned long) &ms_info);
      set_fs(old_fs);
#ifdef DEBUG
      if (i==0) {
	  printk(KERN_INFO "udf: XA disk: %s\n", ms_info.xa_flag ? "yes":"no");
	  printk(KERN_INFO "udf: vol_desc_start = %d\n", ms_info.addr.lba);
      } else
      	  printk(KERN_DEBUG "udf: CDROMMULTISESSION not supported: rc=%d\n",i);
#endif

#define WE_OBEY_THE_WRITTEN_STANDARDS 1

      if (i==0) {
#if WE_OBEY_THE_WRITTEN_STANDARDS
        if (ms_info.xa_flag) /* necessary for a valid ms_info.addr */
#endif
          vol_desc_start=ms_info.addr.lba;
      }
  } else {
	printk(KERN_DEBUG "udf: device doesn't know how to ioctl?\n");
  }
  return vol_desc_start;
}
