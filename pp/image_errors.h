/**
 * @file image_errors.h
 * Report errors in images
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 */

#ifndef IMAGE_ERRORS_H
#define IMAGE_ERRORS_H

#include <list>

class inverted_profile;

/// output why the image passed can't be read to stderr
void report_image_error(inverted_profile const & profile, bool fatal);

/// output why any bad images can't be read to stderr
void report_image_errors(std::list<inverted_profile> const & plist);

#endif /* IMAGE_ERRORS_H */
