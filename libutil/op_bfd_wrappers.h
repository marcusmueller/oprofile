/**
 * @file op_bfd_wrappers.h
 * Wrappers to hide API changes in binutils 2.34
 *
 * @remark Copyright 2020 OProfile authors
 * @remark Read the file COPYING
 *
 * @author William Cohen
 */

#ifndef OP_BFD_WRAPPERS_H
#define OP_BFD_WRAPPERS_H

#if HAVE_BINUTILS_234
#define op_bfd_section_size(ibfd, sec) bfd_section_size(sec)
#define op_bfd_get_section_flags(abfd, sec) bfd_section_flags(sec)
#define op_bfd_set_section_flags(abfd, sec, flags) bfd_set_section_flags(sec, flags)
#define op_bfd_set_section_vma(abfd, sec, vma) bfd_set_section_vma(sec, vma)
#define op_bfd_set_section_size(abfd, sec, size) bfd_set_section_size(sec, size)
#else
#define op_bfd_section_size(ibfd, sec) bfd_section_size(ibfd, sec)
#define op_bfd_get_section_flags(abfd, sec) bfd_get_section_flags(abfd, sec)
#define op_bfd_set_section_flags(abfd, sec, flags) bfd_set_section_flags(abfd, sec, flags)
#define op_bfd_set_section_vma(abfd, sec, vma) bfd_set_section_vma(abfd, sec, vma)
#define op_bfd_set_section_size(abfd, sec, size) bfd_set_section_size(abfd, sec, size)
#endif

#endif /* !OP_BFD_WRAPPERS_H */
