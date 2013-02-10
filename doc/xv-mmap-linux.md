XV Linux mmap implementation | Spencer Tipping
Licensed under the terms of the MIT source code license

    #ifndef XV_MMAP_LINUX_H
    #define XV_MMAP_LINUX_H

    typedef struct {
      void*  addr;
      size_t length;
      int    prot;
      int    flags;
      int    fd;
      off_t  offset;
    } xv_mmap_args_t;

    inline void *xv_mapping_start(xv_mmap_args_t *mapping) {
      return mapping->addr;
    }

    inline size_t xv_mapping_size(xv_mmap_args_t *mapping) {
      return mapping->length;
    }

    #endif