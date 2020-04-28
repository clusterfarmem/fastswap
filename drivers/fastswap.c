#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/frontswap.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/page-flags.h>
#include <linux/memcontrol.h>
#include <linux/smp.h>

#define B_DRAM 1
#define B_RDMA 2

#ifndef BACKEND
#error "Need to define BACKEND flag"
#endif

#if BACKEND == B_DRAM
#define DRAM
#include "fastswap_dram.h"
#elif BACKEND == B_RDMA
#define RDMA
#include "fastswap_rdma.h"
#else
#error "BACKEND can only be 1 (DRAM) or 2 (RDMA)"
#endif

static int sswap_store(unsigned type, pgoff_t pageid,
        struct page *page)
{
  if (sswap_rdma_write(page, pageid << PAGE_SHIFT)) {
    pr_err("could not store page remotely\n");
    return -1;
  }

  return 0;
}

/*
 * return 0 if page is returned
 * return -1 otherwise
 */
static int sswap_load_async(unsigned type, pgoff_t pageid, struct page *page)
{
  if (unlikely(sswap_rdma_read_async(page, pageid << PAGE_SHIFT))) {
    pr_err("could not read page remotely\n");
    return -1;
  }

  return 0;
}

static int sswap_load(unsigned type, pgoff_t pageid, struct page *page)
{
  if (unlikely(sswap_rdma_read_sync(page, pageid << PAGE_SHIFT))) {
    pr_err("could not read page remotely\n");
    return -1;
  }

  return 0;
}

static int sswap_poll_load(int cpu)
{
  return sswap_rdma_poll_load(cpu);
}

static void sswap_invalidate_page(unsigned type, pgoff_t offset)
{
  return;
}

static void sswap_invalidate_area(unsigned type)
{
  pr_err("sswap_invalidate_area\n");
}

static void sswap_init(unsigned type)
{
  pr_info("sswap_init end\n");
}

static struct frontswap_ops sswap_frontswap_ops = {
  .init = sswap_init,
  .store = sswap_store,
  .load = sswap_load,
  .poll_load = sswap_poll_load,
  .load_async = sswap_load_async,
  .invalidate_page = sswap_invalidate_page,
  .invalidate_area = sswap_invalidate_area,

};

static int __init sswap_init_debugfs(void)
{
  return 0;
}

static int __init init_sswap(void)
{
  frontswap_register_ops(&sswap_frontswap_ops);
  if (sswap_init_debugfs())
    pr_err("sswap debugfs failed\n");

  pr_info("sswap module loaded\n");
  return 0;
}

static void __exit exit_sswap(void)
{
  pr_info("unloading sswap\n");
}

module_init(init_sswap);
module_exit(exit_sswap);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Fastswap driver");
