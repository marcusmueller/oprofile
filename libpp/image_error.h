/**
 * @file image_error.h
 * Possible problem with binary images
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 */

#ifndef IMAGE_ERROR_H
#define IMAGE_ERROR_H

/// possible reasons why we can't read a binary image
enum image_error {
	image_ok = 0,
	image_not_found,
	image_unreadable,
	image_format_failure,
	image_multiple_match
};

#endif /* IMAGE_ERROR_H */
