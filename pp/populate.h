/**
 * @file populate.h
 * Fill up a profile_container from inverted profiles
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef POPULATE_H
#define POPULATE_H

class profile_container;
class inverted_profile;


/// Load all sample file information for exactly one binary image.
/// return false if none of the image contains debug information
bool
populate_for_image(profile_container & samples, inverted_profile & ip);

#endif /* POPULATE_H */
