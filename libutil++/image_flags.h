/**
 * @file image_flags.h
 * Possible problem flags with binary images
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 */

#ifndef IMAGE_FLAGS_H
#define IMAGE_FLAGS_H

/// possible reasons why we can't read a binary image
enum image_flags {
	image_ok = 0,
	image_not_found = 1 << 0,
	image_unreadable = 1 << 1,
	image_format_failure = 1 << 2,
	image_multiple_match = 1 << 3
};

#endif /* IMAGE_FLAGS_H*/
